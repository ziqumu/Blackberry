// USBClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "sha/sha.h"
#import "BbDevMgr.exe" no_namespace named_guids
#include "BBDevMgr_errors.h"
#include <conio.h>

#include <crtdbg.h>


#define MAX_PACKET_SIZE 16380

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

//data members
IChannelPtr _channel;
HANDLE _events[NUM_EVENTS];
IDevicePtr _device;
IChannel_300 *channel_300 = NULL;
IDeviceManagerPtr _deviceManager; //(__uuidof(IDeviceManager));

//prototypes
bool Init();
bool Open();
void Close();

//IChannelEvents implementation
class ChannelEvents : public IChannelEvents {
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
        theChar = _getch();

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
            _getch();

        //
        // legit character
        // store it within the password, echo '*' to the screen
        //
        } else {
            password[numChars++] = (char)theChar;
            _putch('*');
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
    ULONG _refcount;
} channelEvents;


//Device Manager Events
class DeviceManagerEvents : public IDeviceManagerNotification {

public:
    SYSTEMTIME lt;

    STDMETHOD(QueryInterface)(REFIID iid, void** pp) {
        if (iid == IID_IDeviceManagerNotification ||
            iid == IID_IDeviceManagerEvents ||
            iid == IID_IUnknown) {

            *pp = (IDeviceManagerNotification*) this;
            return 0;
        }
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() {
        return 1;
    }
    STDMETHOD_(ULONG, Release)() {
        return 1;
    }
    STDMETHOD(raw_DeviceConnect)(/*[in]*/ IDevice* device) {
        device;
        GetLocalTime(&lt);
        printf("Device Connected at %02d:%02d:%02d:%03d\n", lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);
        //give the device time to setup channel
        Sleep(500);
        //reinitialize device
        Init();
        //reopen channel
        Open();
        return 0;
    }
    STDMETHOD(raw_DeviceDisconnect)(/*[in]*/ IDevice* device) {
        device;
        GetLocalTime(&lt);
        printf("Device Disconnected at %02d:%02d:%02d:%03d\n", lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);
        //cable disconnected, so close the channel
        Close();
        return 0;
    }
    STDMETHOD(raw_SuspendRequest)(void) {
        GetLocalTime(&lt);
        printf("Suspend request received at %02d:%02d:%02d:%03d\n", lt.wHour,
                                                                lt.wMinute,
                                                                lt.wSecond,
                                                                lt.wMilliseconds);
        //must close channel or BBDevMgr will cancel suspend request
        Close();
        return ERROR_SUCCESS;
    }
    STDMETHOD(raw_SuspendNotification)(/*[in]*/ long suspending) {
        suspending;
        GetLocalTime(&lt);
        printf("Notification that PC is suspending at %02d:%02d:%02d:%03d\n", lt.wHour,
                                                                        lt.wMinute,
                                                                        lt.wSecond,
                                                                        lt.wMilliseconds );
        return ERROR_SUCCESS;
    }
    STDMETHOD(raw_ResumeNotification)(void) {
        GetLocalTime(&lt);
        printf("Notification that PC is resuming at %02d:%02d:%02d:%03d\n", lt.wHour,
                                                                        lt.wMinute,
                                                                        lt.wSecond,
                                                                        lt.wMilliseconds );
        //reopen the closed channel
        Open();
        return ERROR_SUCCESS;
    }

} deviceManagerEvents;

//Error reporting
void ReportError(HRESULT dwError) {
    LPVOID lpMsgBuf;

    switch(dwError)
    {
        //BBDevMgr Errors
        case ICHANNEL_OPEN_NAK_RESERVED:
            printf("Channel reserved");
            break;
        case ICHANNEL_OPEN_NAK_NOT_SUPPORTED:
            printf("Channel not supported");
            break;
        case ICHANNEL_OPEN_NAK_ALREADY_OPEN:
            printf("Channel already open");
            break;
        case ICHANNEL_OPEN_NAK_MAX_CHANNELS_EXCEEDED:
            printf("Max channels exceeded");
            break;
        case ICHANNEL_OPEN_NAK_REJECTED_BY_APPLICATION:
            printf("Channel open was rejected by application");
            break;
        case ICHANNEL_OPEN_NAK_WAIT_TIMEOUT:
            printf("Channel open timeout");
            break;
        case ICHANNEL_OPEN_NAK_INVALID_PARAMETER:
            printf("Channel open: invalid parameter");
            break;
        case IDEVICE_INVALID:
            printf("Invalid device");
            break;
        case IDEVICE_NOT_SUPPORTED:
            printf("Device not supported");
            break;
        default:
            //Windows Errors
            FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL, dwError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), // Default language
                ( LPTSTR ) & lpMsgBuf, 0, NULL );

            if (lpMsgBuf) {
                printf((char *)lpMsgBuf);
                LocalFree( lpMsgBuf );
            } else {
                //Unknown
                printf("Unknown error 0x%x", dwError);
            }
    }
}

//initialize BBDevMgr device
bool Init () {
        //get the list of attached devices
        IDevicesPtr devices = _deviceManager->Devices;
        //query the count of attached devices
        if ( devices->Count > 0 )
        {
            // This will allow us to get a notification if/when a SUSPEND request is
            // sent to BBDevMgr.
            DWORD cookie;
            HRESULT result = _deviceManager->Advise( &deviceManagerEvents, &cookie );
            if (FAILED(result)) {
                ReportError(result);
                return 1;
            }
            //get the first device
            _device = devices->Item(0);
            return true;
        } else
            //no device attached
            return false;
}


//Open the channel
bool Open() {
    //create some events for listening for notifications from the device
    _events[READ_EVENT] = CreateEvent(NULL, FALSE, FALSE, NULL); //unnamed
    _events[CLOSE_EVENT] = CreateEvent(NULL, TRUE, FALSE, NULL); //unnamed
    if( _events[READ_EVENT] == INVALID_HANDLE_VALUE || _events[CLOSE_EVENT] == INVALID_HANDLE_VALUE ) {
        printf("Invalid handle\n");
        return false;
    }

    printf("\nOpening Channel: %s\n", CHANNEL);
    //open a channel
    try {
        _channel = _device->OpenChannel(CHANNEL, new ChannelEvents());
    } catch(_com_error e) {
        printf("\nChannel couldn't be opened!\nMake sure the USB app is running on the device\n\n");
        return false;
    }
    printf("Channel Open: %s\n", CHANNEL);
    return true;
}

//Close the channel
void Close()
{
    //release IChannel_300 if we're using it
    if (channel_300 != NULL) {
        channel_300->Release();
        channel_300 = NULL;
    }
    //close channel if open
    if (_channel != NULL) {
        _channel = NULL;

        // Wait for BbDevMgr to close the channel, otherwise it ends up crashing.
        WaitForSingleObject( _events[CLOSE_EVENT], 5000 );

        CloseHandle( _events[READ_EVENT] );
        CloseHandle( _events[CLOSE_EVENT] );
    }
}

//Gets the properties of the device
void DumpProperties() {
    IDevicePropertiesPtr properties = _device->Properties;

    int count = properties->Count;
    printf ("Device Properties:\n");
    for (int i=0; i<count; i++) {
        IDevicePropertyPtr property = properties->Item[(long)i];

        _variant_t val = property->Value;

        char name[256];
        memset( name, 0, sizeof( name ) );
        WideCharToMultiByte( CP_ACP, 0, property->Name, SysStringLen( property->Name ),
                    name, sizeof( name ), NULL, NULL );

        if( wcscmp(property->Name, L"BBPIN") == 0 ) { //BBPIN
            printf("%s = %X\n", name, val.lVal);
        }
        else if ( wcscmp(property->Name, L"Vendor ID") == 0 ) { //Vendor ID
            printf("%s = %d\n", name, (WORD)(val.uiVal) );
        }
        else if ( wcscmp(property->Name, L"MUX Version") == 0 ) { //MUX Version (DWORD = wMajor.wMinor)
            val.ChangeType(VT_UI4);
            printf("%s = %d.%d\n", name, val.intVal >> 8, val.intVal & 0xff);
        }
        else if ( (wcscmp(property->Name, L"GUID") == 0) || (wcscmp(property->Name, L"NetworkType") == 0) ) { //otherwise
            val.ChangeType(VT_BSTR);
            printf("%s = %ls\n", name, val.bstrVal);
        }
    }
}

void ComError(_com_error e)
{ 
    //::MessageBox(NULL, e.ErrorMessage(), "USBClient ERROR", MB_ICONERROR | MB_OK );
}

//Receives a packet
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

//Decodes recieved packet if using GCF
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

//Sends a packet
HRESULT Send(char * buf)
{
    printf("Sending: %s\n", buf);
    HRESULT result = _channel->WritePacket((unsigned char*)buf, strlen(buf));
    return ( result );
}

/**
 * The usb client. This opens a raw connection to the device, it is expected that an application is waiting on the device to
 * interact with this desktop process
 */
int USBClient () {

    try {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);

        //grab an instance of the main proxy object: IDeviceManager
        HRESULT result = _deviceManager.CreateInstance(CLSID_DeviceManager);
        if (FAILED(result)) {
            ReportError(result);
            return 1;
        }

        //Initialize connection to BBDevMgr device
        if ( Init() ) {

        //get the properties for the attached device
        DumpProperties();

        //Open channel
        if ( !Open() ) {
            return 1;
        }

        //IChannel_300 support
        LONG pkts, bytes;
        HRESULT result = _channel.QueryInterface( IID_IChannel_300, (void **)&channel_300 );
        if (FAILED(result)) {
            ReportError(result);
            return 1;
        }
        //Shows queued packets (will be 0 if device is consuming as we produce)
        channel_300->GetWriteQueueInfo( &pkts, &bytes );
        printf("\nQueue Info:\n");
        printf("  Packets:    %d\n", pkts);
        printf("  Bytes:      %d\n", bytes);


        //Shows BBDevMgr parameters
        ChannelParams params;
        _channel->GetParams( &params );
        printf("\nChannel Parameters:\n");
        printf("  maxReceiveUnit:     %d\n", params.maxReceiveUnit );
        printf("  maxTransmitUnit:    %d\n", params.maxTransmitUnit );
        printf("  deviceBuffers:      %d\n", params.deviceBuffers );
        printf("  hostBuffers:        %d\n", params.hostBuffers );
        printf("  droppedPacketCount: %d\n\n", params.droppedPacketCount );


        char buffer[MAX_PACKET_SIZE];
        char * pString = NULL;

        long len = 0;
        strcpy_s(buffer, PC_HELLO);

        //send something
        result = Send(buffer);

        if (FAILED(result)) {
            ReportError(result);
            return 1;
        }

        //receive something
        if ( NULL != (pString = (ReadVariableProtocol(DEVICE_HELLO, buffer, sizeof(buffer), len )) ))
        {
            if ( strncmp(pString, DEVICE_HELLO, len) != 0 )
            {
            printf("Failed to receive proper string from device\n");
            }
            else
            {
            for (int i=0; i<5; i++) {
                sprintf(buffer, "Q packet %d", (i+1));
                result = Send(buffer);
            }
            //Shows queued packets (will be 0 if device is consuming as we produce)
            channel_300->GetWriteQueueInfo( &pkts, &bytes );
            printf("\nQueue Info:\n");
            printf("  Packets:    %d\n", pkts);
            printf("  Bytes:      %d\n", bytes);


            //Give user a chance to test standby functionality
            printf("Allowing user to enter standby or disconnect/reconnect cable\n");
            printf("Press any key to resume...\n");
            while( !_kbhit() ) {
                Sleep( 100 );
            }
            _getch();

            //send something
            strcpy_s(buffer, PC_GOODBYE);
            Send(buffer);

            //receive something
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

        //close channel
        Close();
        } else {
            //device count was 0
            printf("Device not connected\n");
        }
    } catch ( _com_error e) {
        ComError(e);
    }

    return 1;
}

int main(int argc, char* argv[])
{
    int temp;
    argc;
    argv;
    temp = USBClient();

    printf("Press any key to continue...\n");
    while( !_kbhit() ) {
        Sleep( 100 );
    }
    return temp;
}
