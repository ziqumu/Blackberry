// USBClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "sha/sha.h"
#import "BBDevMgr.exe" no_namespace named_guids
#include <conio.h>

enum{ READ_EVENT, CLOSE_EVENT, NUM_EVENTS};

//using the GCF framework on the device, the length of the inbound data is prepended to the binary stream
// therefore, use this struct to decode the data
//HOWEVER, in the case of the lowlevel data (net.rim.device.api.system.USBPort), the length bytes are NOT prepended
typedef struct {
    unsigned short length;
    unsigned char data[256];
} DEVICE_DATA;

static const int HEADERSIZE = sizeof(unsigned short);

static char const * const PC_HELLO = "Hello from PC";
static char const * const PC_GOODBYE = "Goodbye from PC";
static char const * const DEVICE_HELLO = "Hello from Device";
static char const * const DEVICE_GOODBYE = "Goodbye from Device";
#define CHANNEL L"JDE_USBClient"
/**
 * The simple usb client. This opens a raw connection to the device, it is expected that an application is waiting on the device to
 * interact with this desktop process
 */
class USBClient:
    public IChannelEvents //implements the ICHannelEvents interface in order to receive notifications on the connection to the device
{
public:
    USBClient()
    {
    }

    ~USBClient()
    {}

public:

    int run()
    {

	//create some events for listening for notifications from the device
	_events[READ_EVENT] = CreateEvent(NULL, FALSE, FALSE, NULL); //unnamed
	_events[CLOSE_EVENT] = CreateEvent(NULL, TRUE, FALSE, NULL); //unnamed
	if( _events[READ_EVENT] == INVALID_HANDLE_VALUE || _events[CLOSE_EVENT] == INVALID_HANDLE_VALUE ) {
	    return false;
	}

	try {
	    CoInitializeEx(NULL, COINIT_MULTITHREADED);

	    //grab an instance of the main proxy object: IDeviceManager
	    IDeviceManagerPtr deviceManager; //(__uuidof(IDeviceManager));
	    deviceManager.CreateInstance(CLSID_DeviceManager);

	    //get the list of attached devices
	    IDevicesPtr devices = deviceManager->Devices;

	    //query the count of attached devices
	    if ( devices->Count > 0 )
	    {
		IDevicePtr device = devices->Item(0); //get the first

		//get the properties for the attached device
		IDevicePropertiesPtr properties = device->Properties;
		long count = properties->Count;
		for (long i = 0; i < count; ++i)
		{
		    IDevicePropertyPtr property = properties->Item[i]; //(_variant_t(i));

		    char name[256];
		    memset( name, 0, sizeof( name ) );
		    WideCharToMultiByte( CP_ACP, 0, property->Name, SysStringLen( property->Name ),
                        name, sizeof( name ), NULL, NULL );

		    printf("Name: %s\n",
			name);

		    _variant_t var(property->Value);
		    if ( strcmp("BBPIN", name) == 0 )
		    {
			long pin = (long)(var);
			printf("Value: 0x%x\n", pin);
		    }
		    else
		    {
			printf("Value: %s\n",
			(char*)((_bstr_t)var));
		    }

		}

		//open a channel and exchange some data
		printf("");
		printf("Opening Channel: %s\n", CHANNEL);
		_channel = device->OpenChannel(CHANNEL, this);

		char buffer[258];
		char * pString = NULL;

		long len = 0;
		strcpy(buffer, PC_HELLO);

		Send(buffer);

		if ( NULL != (pString = (ReadVariableProtocol(DEVICE_HELLO, buffer, sizeof(buffer), len )) ))
		{
		    if ( strncmp(pString, DEVICE_HELLO, len) != 0 )
		    {
			printf("Failed to receive proper string from device\n");
		    }
		    else
		    {
			strcpy(buffer, PC_GOODBYE);
			Send(buffer);

			if ( NULL != (pString = (ReadVariableProtocol(DEVICE_GOODBYE, buffer, sizeof(buffer), len))))
			{
			    if ( strncmp(pString, DEVICE_GOODBYE, len) != 0 )
			    {
				printf("Failed to receive proper string from device\n");
			    }
			    printf("Success!\n");
			}
		    }
		}

		Close();
	    }
	} catch ( _com_error e) {
	    ComError(e);
	}

	return 1;
    }

private:

    char * ReadVariableProtocol(char const * match, char * buffer, int bufferLen, long & outputLen)
    {
	char * p = NULL;
	if ( Receive((unsigned char*)buffer, bufferLen, outputLen) )
	{
	    //figure out if the length bytes are leading
	    if ( strncmp(buffer, match, HEADERSIZE) != 0)
	    {
		//we're using the GCF, and we have length bytes prepended to the start
		p = (char *)((DEVICE_DATA*)buffer)->data;
		outputLen = outputLen - HEADERSIZE;
	    }
	    else
	    {
		p = buffer;
	    }
	    printf("Received: %s\n", p);
	}
	return p;
    }


    bool Send(char * buf)
    {
	printf("Sending: %s\n", buf);
	HRESULT result = _channel->WritePacket((unsigned char*)buf, strlen(buf));
	return ( !FAILED(result));
    }

    bool Receive(unsigned char * buf, int len, long & actual)
    {
	switch( WaitForMultipleObjects( NUM_EVENTS, _events, FALSE, 30000 ) ) { //30 sec timeout
	    case WAIT_OBJECT_0:
		try {
		    HRESULT result = _channel->ReadPacket( buf, len, &actual );

		    return( !FAILED( result ) );
		} catch( _com_error e ) {
		    ComError( e );
		    return false;
		}
		break;
	    case WAIT_OBJECT_0 + 1:
		// Closed
		printf("Port closed\n" );
		return false;
	    case WAIT_TIMEOUT:
		printf("Timeout waiting for inbound data\n");
		return false;
	    default:
		// Shouldn't happen
		return false;
	}
    }

    void Close()
    {
	//close the channel and wait for BBDevMgr to shut it down
	_channel = NULL;

	// Wait for BbDevMgr to close the channel, otherwise it ends up crashing.
	WaitForSingleObject( _events[CLOSE_EVENT], 5000 );

	CloseHandle( _events[READ_EVENT] );
	CloseHandle( _events[CLOSE_EVENT] );
    }

    void ComError(_com_error e)
    {
	::MessageBox(NULL, e.ErrorMessage(), "USBClient ERROR", MB_ICONERROR | MB_OK );
    }


public:
    //implementation of the IChannelEvents interface
    virtual HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void **ppvObject )
    {
	if ( riid == IID_IChannelEvents || riid == IID_IUnknown) {
	    *ppvObject = (IChannelEvents*)this;
	    AddRef();
	    return 0;
	}
	return E_NOINTERFACE;
    }

    virtual ULONG   STDMETHODCALLTYPE AddRef()
    {
	return ++_refcount;
    }

    virtual ULONG   STDMETHODCALLTYPE Release()
    {
	ULONG count = --_refcount;
	if ( count == 0 )
	{
	    delete this;
	}
	return count;
    }

    virtual HRESULT STDMETHODCALLTYPE raw_CheckClientStatus( void )
    {
	return 0; //not implemented
    }

        /**
     * Called by a locked device to query for the password from the client
     * This method must write the sha_hash of the password into the provided array and return S_OK;
     * return E_FAIL to indicate an error
     */
    virtual HRESULT STDMETHODCALLTYPE raw_OnChallenge(
        /* [in] */ long attemptsRemaining,
        /* [out] */ unsigned char passwordHash[20] )
    {
	attemptsRemaining;
	char   password[20];
	int    passwordMax = 14;
	int    exit     = 0;
	int    theChar  = 0;
	int    numChars = 0;
	static int count = 0;

	printf( "Attempt %d of 10 to enter password.\n", ++count );
	printf( "Note that the device will be wiped out after 10 failed attempts.\n\n" );
	printf( "Enter BlackBerry password: " );

	do
	{
		theChar = getch();

		//
		// user hit Enter to exit
		//
		if ( theChar == 0x0D || theChar == 0x0A )
		{
			++exit;

		//
		// returns 0 if a function or cursor key has been hit
		// get next char to clear the scan code
		//
		} else if ( !theChar ) {
			getch();

		//
		// legit character
		// store it within the password, echo '*' to the screen
		//
		} else {
			password[numChars++] = (char)theChar;
			putch('*');
		}
	} while ( !exit && numChars < passwordMax );

	password[numChars] = 0;

	printf( "\n");

	sha_hash(password, strlen(password), passwordHash);
	return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE raw_OnNewData( void )
    {
	SetEvent(_events[READ_EVENT]);
	return 0;
    }

    virtual HRESULT STDMETHODCALLTYPE raw_OnClose( void )
    {
	SetEvent(_events[CLOSE_EVENT]);
	return 0;
    }

private:
    //data members
    IChannelPtr _channel;
    ULONG _refcount;
    HANDLE _events[NUM_EVENTS];

};

int main(int argc, char* argv[])
{
    argc;
    argv;
    USBClient usb;
    return usb.run();
}
