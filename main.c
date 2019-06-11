#include "hw_md.h"
#include "scancodeTable.h"

unsigned char globalCounter = 0;

void ReadCharacters();

void WaitForVBlank();
void readControllers();
void readKeyboard();

short GetHandshakeNibblePort2( short* hshkState );
void PutHandshakeNibblePort2( short* hshkState, unsigned char byteToSend );

int FindESKeyboard( void );
void ReadESKeyboard ( void );
void WriteESKeyboard ( void );
void SendCmdToESKeyboard( unsigned char *cmdBuf, unsigned char cmdLen );
unsigned char GetNextHardwareKeyboardChar( void );
void EmulateJoypadWithKeyboard( void );
long GetHardwareKeyboardFlags( void );
void SetHardwareKeyboardFlags( long flags );
unsigned char GetNextESKeyboardChar( void );
unsigned char GetNextESKeyboardRawcode( void );
void BackUpKeycodeTail();

void _external_callback();
void _vint_callback();

char keyboardConnected = 0;

char aButtonPressed = 0;
char bButtonPressed = 0;
char cButtonPressed = 0;
char startButtonPressed = 0;

unsigned char scancodeTableSize = sizeof(scancodeToAscii) / sizeof(unsigned char);

ControlGlobals ControlGlobalz;

unsigned char charBuff[16];
char charBuffIndex = 0;



typedef struct 
{ 
   short Ycoor;
   char widthHeight;
   char indexOfNext;
   char hvFlipping;
   char tileIndex;
   short Xcoor;
} CursorDesc;



extern long xCursor;
extern long yCursor;
extern long vScrollPos;





volatile unsigned char *SERIAL_CONTROL_P1 = (char*) 0xA10013;
volatile unsigned char *RX_DATA_P1 = (char*) 0xA10011;
volatile unsigned char *TX_DATA_P1 = (char*) 0xA1000F;

/*
long * SERIAL_CONTROL_P2 = (long *) 0xA10019;
long * RX_DATA_P2 = (long *) 0xA10017;
long * TX_DATA_P2 = (long *) 0xA10015;

long * SERIAL_CONTROL_P3 = (long *) 0xA1001F;
long * RX_DATA_P3 = (long *) 0xA1001D;
long * TX_DATA_P3 = (long *) 0xA1001B;
*/


unsigned char SERIAL_1200BPS = 0b10 << 6;
unsigned char SERIAL_ENABLE = 0b11 << 4;
unsigned char SERIAL_INTOK = 0b1 << 3;





int main(void)
{
    // 0x0000 = grey
    // 0x2000 = green
    // 0x4000 = red


    // xCursor = 32;


	WaitForVBlank();
    
    
    // 0b 10 11 10 00  =  1200 bps, enable serial mode on TR and TL, enable external interrupt, nothing
    // *SERIAL_CONTROL_P1 = 0b10111000;

    *SERIAL_CONTROL_P1 = SERIAL_1200BPS|SERIAL_ENABLE|SERIAL_INTOK;
    
    

    while ( 1 ) // endless loop
    {	
		ReadCharacters();
    }
	
    return 0;
}






void _external_callback()   // Called during External Interrupt
{
    // syscall_PRINT_STRING("a", 0x0000);
    
    // check if a byte is available:

    register long timeout = 100;
    
    do { }
    while (  ( ( (*SERIAL_CONTROL_P1 >> 1 ) & 0b00000011 ) != 0b01 ) && --timeout );

    if ( !timeout )
    {
        return;
    }
    
    // add the byte to the buffer
    if( charBuffIndex < 15 )
    {
        charBuff[charBuffIndex] = *RX_DATA_P1;
        charBuffIndex++;
        charBuff[charBuffIndex] = '\0';
    }
            
}






CursorDesc cursorDesc = { 0x100, 0b00000000, 0x00, 0x00, 0xFF, 0x8F };

long * VDP_DATA = (long *) 0xC00000;
long * VDP_CONTROL = (long *) 0xC00004;
long * VDP_WRITE_SPRITE_TABLE = (long *) 0x68000002;


long * cursorPointer;




void _vint_callback()	// Called During V-Blank Interrupt
{
    readControllers();
    readKeyboard();

	if( globalCounter >= 16 ) globalCounter = 0;
	else globalCounter++;

    if( charBuffIndex > 0 )
    {
        syscall_PRINT_STRING(charBuff, 0x0000);
        charBuffIndex = 0;
    }



    cursorDesc.Xcoor = ( xCursor * 8 ) + 128;
    cursorDesc.Ycoor = ( yCursor * 8 ) + 128 - vScrollPos;

    // if subtracting vScrollPos causes cursor to be drawn off screen
    // draw the cursor at the bottom of the screen
    if( cursorDesc.Ycoor < 128 )
        cursorDesc.Ycoor = (25 * 8) + 128;


    if( globalCounter < 8 )
        cursorDesc.tileIndex = 0xFF;
    else
        cursorDesc.tileIndex = 0x0;



    cursorPointer = &cursorDesc;
    *VDP_CONTROL = VDP_WRITE_SPRITE_TABLE;
    *VDP_DATA = *cursorPointer;
    cursorPointer++;
    *VDP_DATA = *cursorPointer;

}






void ReadCharacters()
{
    short command = 0;

    unsigned char keyPress = 0;
    short result = 0;
    unsigned char HWKeypress;
    long hwflags;

    HWKeypress = GetNextHardwareKeyboardChar();

    switch( HWKeypress )
    {
        case 0x08:                              // BS?
                command = kButtonC;
                break;
                
        case 0x09:                              // Tab?
                command = kButtonA;
                HWKeypress = keySwitch;
                break;
                
        case 0x0A:  
                command = kButtonA;
                HWKeypress = '\n';
                break;
                
        case 0x1B:                              // ESC?
                command = kButtonA;
                HWKeypress = keyCancel;
                break;
                                
        default:
                if ( HWKeypress != kNoKey )
                    command = kButtonA;         // fake a button down
                else
                {
                    hwflags = GetHardwareKeyboardFlags();
                    if ( hwflags & ( kShiftDown | kCapsLocked ) )
                    {
                        HWKeypress = keyShift;     
                    }
                }
                break;
    }
    
    // Did the user click the button?
    if (command & (kButtonA | kButtonB | kButtonC | kStart))
    {

        if (command & kButtonA)
        {
            // User typed a character. Find out which key was hit
            if ( HWKeypress != kNoKey )     // did she type it on the hardware keybd?
                keyPress = HWKeypress;
        }
        
        // Was it a letter, or a special?
        if (keyPress == '\n' || keyPress >= ' ')
        {





/*

            if( charBuffIndex < 15 )
            {
                charBuff[charBuffIndex] = keyPress;
                charBuffIndex++;
                charBuff[charBuffIndex] = '\0';
            }
            
*/

            // !!!=====================================================!!!
            
            // Instead of adding keyPress to charBuff like above,
            // serial write it to the PC
            
            // !!!======================================================!!!
            
            
           
            // poll bit 0 of control register until it is equal to 0
            // write the byte to the TxData register
            
            register long timeout = 100;
    
            do { }
            while (  ( ( *SERIAL_CONTROL_P1 & 0b00000001 ) != 0b0 ) && --timeout );

            if ( !timeout )
            {
                return;
            }
            
            *TX_DATA_P1 = keyPress;
            
            
            


            // =======================================

			// drawCharToWindow(console.self, keyPress);

            // ADD CHAR TO A BUFFER HERE
            // THEN DRAW DA BUFFER DURING VBLANK AND
            // CLEAR BUFFER AFTER DRAW

            // =======================================



        }
        else
        {
            // Special
            switch (keyPress)
            {
                case keyDelete:
  
                        break;
                        
                case keyCancel:

                        break;
                
                case keyShift:

                        break;
                        
                case keyReturn:

                        keyPress = '\n';
                        break;
                        
                case keySwitch:

                        break;
                
                case keyDone:

                        break;
            }
        }
    }

}


void readControllers() {

    short buttons = 0;
    buttons = get_pad(0);

    // check A button
    if( buttons & SEGA_CTRL_A )
    {
        if( !aButtonPressed ) {

            syscall_PRINT_STRING("waffles", 0x0000);

            aButtonPressed = 1;
        }
    }
    else
    {
        aButtonPressed = 0;
    }

    // check B button
    if ( buttons & SEGA_CTRL_B )
    {
        if( !bButtonPressed ) {

            syscall_SCROLL();

            bButtonPressed = 1;
        }
    }
    else
    {
        bButtonPressed = 0;
    }

    // check C button
    if ( buttons & SEGA_CTRL_C )
    {
        if( !cButtonPressed ) {

            if( keyboardConnected )
            {
                put_str("CAPS", 0x0000, 13, 5);

                unsigned char fuck[2];        
                ControlGlobalz.keyboardFlags ^= kCapsLocked;            // flip caps lock state
                fuck[0] = 0xED;                                         // hit the LED reg
                fuck[1] = ControlGlobalz.keyboardFlags & kCapsLocked;   // bits [2:0] are caps/num/scroll lock
                SendCmdToESKeyboard( fuck, 2 );
            }

            cButtonPressed = 1;
        }
    }
    else
    {
        cButtonPressed = 0;
    }

    // check Start button
    if ( buttons & SEGA_CTRL_START )
    {
        if( !startButtonPressed ) {

            startButtonPressed = 1;
        }
    }
    else
    {
        startButtonPressed = 0;
    }
}



void readKeyboard() {

    if ( FindESKeyboard() )
	{
		if (!keyboardConnected)
		{
			unsigned char fuck;
			fuck = 0xFF;						/* reset keyboard */
			SendCmdToESKeyboard( &fuck, 1 );
			WriteESKeyboard();
			keyboardConnected = 1;
			ControlGlobalz.keyboardFlags = 0L;
		} 
	
        // put_str("Found ES Keyboard!", 0x2000, 19, 3);

        ReadESKeyboard();
        WriteESKeyboard();
        EmulateJoypadWithKeyboard();
    }
	else
	{
        keyboardConnected = 0;
		// put_str("Keyboard not found", 0x4000, 19, 3);
	}
}



typedef unsigned char       UChar;
typedef unsigned long       ULong;

#define kData2      0xA10005
#define kCtl2       0xA1000B
#define kSerial2    0xA10019

#define kTH         0x40            // controller mode control line
#define kTR         0x20            // controller handshake request
#define kTL         0x10            // controller handshake acknowledge

#define kDataLines  0x0F            // data lines for 3-line hshk communication

#define nop          __asm__ __volatile__ ("nop\n\t");

int FindESKeyboard(void) {
    UChar readBuf[4];
    register UChar*     readScan = readBuf;
    volatile register UChar* reg = (UChar*) kData2;     // only support keyboard on PORT 2!
    short hshkState;
    register long timeout = 100;
    register ULong kbID = 0xC030609;

    *(reg) = kTH + kTR;                                 // both flags hi
    *(char *)kSerial2 = 0;                              // clear serial modes
    *(char *)kCtl2 = kTH + kTR;                         // both flags are outputs now

    nop; nop;
    *readScan++ = *reg & 0x0F;                          // 1st nybble = identifying ID

    *reg = kTR;                                         // turn off TH to start hshk seq

    do { *readScan = *reg & 0x0F; }
    while ( (*readScan != ((kbID >> 16) & 0xF)) && --timeout );     // 2nd nybble has no handshake

    if ( !timeout ) {
        *reg = kTH + kTR;                               // make sure we leave with TH & TR hi
        return ( 0 );
    }

    readScan++;

    hshkState = 0;                                      // start flipping TR
    *readScan++ = GetHandshakeNibblePort2(&hshkState);  // 3rd nybble = local ID
    *readScan = GetHandshakeNibblePort2(&hshkState);    // 4th nybble = local ID

    *reg |= kTH;                                        // abort the transaction

    for ( timeout = 0; timeout != 50; timeout++ );      // spin a bit

    *reg = kTH + kTR;                                   // make sure we leave with TH & TR hi

    if ( *(ULong *) readBuf == kbID )                   // found a good Eric Smith Keyboard
        return (1);
    else
        return (0);
}


#define kESKeycodeData          0
#define kESControllerEcho       1
#define kESControllerVersion    2

void ReadESKeyboard ( void )
{
                        UChar       readBuf[4];
    register            UChar*      readScan = readBuf;
    volatile register   UChar*      reg = (UChar*) kData2;      // only support keyboard on PORT 2!
                        short       hshkState;
                        UChar       len;
                        UChar       temp;
    register            long        timeout = 100;
    register            ULong       kbID = 0xC030609;

    *(reg)              = kTH + kTR;                        // both flags hi
    *(char *)kSerial2   = 0;                                // clear serial modes
    *(char *)kCtl2      = kTH + kTR;                        // both flags are outputs now

    nop; nop;
    *readScan++ = *reg & 0x0F;                              // 1st nybble = identifying ID

    *reg = kTR;                                             // turn off TH to start hshk seq

    do {
        *readScan = *reg & 0x0F;
    }
    while ( (*readScan != ((kbID >> 16) & 0xF)) && --timeout );     // 2nd nybble has no handshake

    if ( !timeout )
    {
        *reg = kTH + kTR;                               // make sure we leave with TH & TR hi
        return;
    }
	
    readScan++;

    hshkState = 0;                                          // start flipping TR
    *readScan++ = GetHandshakeNibblePort2(&hshkState);      // 3rd nybble = local ID
    *readScan++ = GetHandshakeNibblePort2(&hshkState);      // 4th nybble = local ID

    if ( *(ULong *) readBuf == kbID )                       // found a good Eric Smith Keyboard
    {
        len = GetHandshakeNibblePort2(&hshkState);          // 5th nybble = BYTE count, 0-15

        if (len)
        {
            temp = GetHandshakeNibblePort2(&hshkState);             // get data type
            temp <<= 4;
            temp |= GetHandshakeNibblePort2(&hshkState);

            len--;                                                  // len includes data type byte

            if ( temp == kESKeycodeData )
            {                                                       // Key Codes follow
                readScan = ControlGlobalz.keycodeBuf;
                while ( len )
                {                 
                    ControlGlobalz.keycodeHead ++;                                  // bump head 
                    ControlGlobalz.keycodeHead &= kKeybdDataFifoMask;               // circular buf
                    temp = GetHandshakeNibblePort2(&hshkState);
                    temp <<= 4;
                    temp |= GetHandshakeNibblePort2(&hshkState);

                    readScan[ControlGlobalz.keycodeHead] = temp;
                    len--;
                }
            }
            else
            {                                                       // Status Bytes follow
                readScan = ControlGlobalz.statusBuf;
                while ( len )
                {                        
                    ControlGlobalz.statusHead ++;                                       // bump head 
                    ControlGlobalz.statusHead &= kKeybdCmdStatusFifoMask;               // circular buf
                    temp = GetHandshakeNibblePort2(&hshkState);
                    temp <<= 4;
                    temp |= GetHandshakeNibblePort2(&hshkState);

                    readScan[ControlGlobalz.statusHead] = temp;
                    len--;
                }
            }
        }

        *reg = kTH + kTR;                               // make sure we leave with TH & TR hi
    }
}



/*
    
    3   C   6   0   <len>   <hi nyb>   <low nyb> ....
                ^   \____________________________..../
                |        Same Format as Dat/Status
                |
                Driven to tell Eric I have something for him
                
*/

    
void WriteESKeyboard ( void )
{
                    UChar       readBuf[4];
register            UChar*      readScan = readBuf;
volatile register   UChar*      reg = (UChar*) kData2;          // only support keyboard on PORT 2!
                    short       hshkState;
register            long        timeout = 100;
register            ULong       kbID = 0xC030609;
                    UChar       byteToSend;
    
    if( ControlGlobalz.cmdTail != ControlGlobalz.cmdHead )
    {   
        *(reg)              = kTH + kTR;                        // both flags hi
        *(char *)kSerial2   = 0;                                // clear serial modes
        *(char *)kCtl2      = kTH + kTR;                        // both flags are outputs now
    
        nop; nop;
        *readScan++ = *reg & 0x0F;                              // 1st nybble = identifying ID

        *reg = kTR;                                             // turn off TH to start hshk seq

        do {
            *readScan = *reg & 0x0F;
            }
            while ( (*readScan != ((kbID >> 16) & 0xF)) && --timeout );     // 2nd nybble has no handshake

        if ( !timeout )
        {
            *reg = kTH + kTR;                                   // make sure we leave with TH & TR hi
            return;
        }
		
        readScan++;                         
    
        hshkState = 0;                                          // start flipping TR
        *readScan++ = GetHandshakeNibblePort2(&hshkState);      // 3rd nybble = local ID


        if ( (*(ULong *) readBuf & 0xFFFFFF00) == (kbID & 0xFFFFFF00) )     // found a good Eric Smith Keyboard?
        {
            *reg &= 0xF0;                                       // ensure data lines are 0
            *(char *)kCtl2 |= kDataLines;                       // 4 data lines are outputs now

            ControlGlobalz.cmdTail ++;
            ControlGlobalz.cmdTail &= kKeybdCmdStatusFifoMask;
            byteToSend = ControlGlobalz.cmdBuf[ControlGlobalz.cmdTail];

            PutHandshakeNibblePort2(&hshkState, 0);             // 4th nybble = 0 ==> I'm talking to him
            PutHandshakeNibblePort2(&hshkState, 2);             // 2 bytes follow; type & data
            
            PutHandshakeNibblePort2(&hshkState, ((kESKeycodeData & 0xF0)>>4));      
            PutHandshakeNibblePort2(&hshkState, (kESKeycodeData & 0x0F));         // 1st byte = type
            
            PutHandshakeNibblePort2(&hshkState, ((byteToSend & 0xF0)>>4));      
            PutHandshakeNibblePort2(&hshkState, (byteToSend & 0x0F));            // 2nd byte = data

            *(char *)kCtl2 &= ~kDataLines;                      // 4 data lines are back to being inputs
            *reg = kTH + kTR;                                   // make sure we leave with TH & TR hi
        }
    }
        
}



short GetHandshakeNibblePort2( short* hshkState )
{
    register            long        timeout = 100;
    volatile register   UChar*      reg = (UChar*) kData2;

    if (*hshkState == -1)   // timed out, abort (see below)
	{
        return 0x0F;
	}

    if ((*hshkState ^= 1) == 0)
    {
        *reg |= kTR;            // raise TR, wait for TL high
        nop; nop;
        do {}
        while ( !(*reg & kTL) && --timeout );
        if (timeout)
        {
            nop; nop;
            return *reg & 0x0F;
        }
    }
    else
    {
        *reg &= ~kTR;           // lower TR, wait for TL low
        nop; nop;
        do {}
        while ((*reg & kTL) && --timeout);
        if (timeout)
        {
            nop; nop;
            return *reg & 0x0F;
        }
    }

    // if we got this far, we've timed out. return 0xFFs to abort.

    *hshkState = -1;
    return 0xFF;
}




/* By the time we get here, the 4 data lines have been set up as outputs.
   The caller must ensure that there is nothing in the upper nybble of byteToSend.
 */

void PutHandshakeNibblePort2( short* hshkState, unsigned char byteToSend )
{
    register            long        timeout = 100;
    volatile register   UChar*      reg = (UChar*) kData2;

    if (*hshkState == -1)   // timed out, abort (see below)
	{
        return;
	}

    *reg = (*reg & 0xF0) | byteToSend;  // up to caller to be sure nothing is in hi nybble

    if ((*hshkState ^= 1) == 0)
    {
        *reg |= kTR;                    // raise TR, wait for TL high
        nop; nop;
        do {}
        while (!(*reg & kTL) && --timeout);
        if ( timeout )
		{
            return;
		}
    }
    else
    {
        *reg &= ~kTR;                   // lower TR, wait for TL low
        nop; nop;
        do {}
        while ((*reg & kTL) && --timeout);
        if ( timeout )
		{
            return;
		}
    }

    // if we got this far, we've timed out. return 0xFFs to abort.
	
    *hshkState = -1;
}



void SendCmdToESKeyboard( unsigned char *cmdBuf, unsigned char cmdLen )
{
unsigned short oldSR;

    while ( cmdLen )
    {
        ControlGlobalz.cmdHead ++;
        ControlGlobalz.cmdHead &= kKeybdCmdStatusFifoMask;
        ControlGlobalz.cmdBuf[ControlGlobalz.cmdHead] = *cmdBuf;

        cmdBuf++;       
        cmdLen--;
    }
}


unsigned char GetNextHardwareKeyboardChar( void )
{
    if( ControlGlobalz.sysKeysTail != ControlGlobalz.sysKeysHead )
    {
        ControlGlobalz.sysKeysTail ++;
        ControlGlobalz.sysKeysTail &= kSysKeysFifoMask;
        return ControlGlobalz.sysKeysBuf[ControlGlobalz.sysKeysTail];
    }
    else
        return( kNoKey );
}


void EmulateJoypadWithKeyboard( void )
{
    unsigned char key;

    key = GetNextESKeyboardChar();

    ControlGlobalz.sysKeysHead ++;                 /* not pad-like, stick in key buf */
    ControlGlobalz.sysKeysHead &= kSysKeysFifoMask;
    ControlGlobalz.sysKeysBuf[ControlGlobalz.sysKeysHead] = key;
}



long GetHardwareKeyboardFlags( void )
{
	
    return ControlGlobalz.keyboardFlags;
}



void SetHardwareKeyboardFlags( long flags )
{
    unsigned char fuck[2];
    ControlGlobalz.keyboardFlags = flags;
    fuck[0] = 0xED;                     // hit the LED reg
    fuck[1] = flags & 0x7;              // bits [2:0] are caps/num/scroll lock
    SendCmdToESKeyboard( fuck, 2 );
}



void BackUpKeycodeTail( void )
{
    if( ControlGlobalz.keycodeTail == 0 )                   // back up; we'll try again later
        ControlGlobalz.keycodeTail = kKeybdDataFifoMask;
    else
        ControlGlobalz.keycodeTail --;
}



unsigned char GetNextESKeyboardChar( void )
{
    unsigned char raw;
    unsigned char map;
    unsigned long offset;
    unsigned char fuck[2];
    char       specialPending;
    
    specialPending = 0;
    map = kNoKey;
    
    if ( scancodeToAscii )
    {
        raw = GetNextESKeyboardRawcode();           // get next code out of buffer
        
        //
        // BREAK (KEYUP) CODES
        //
        
        if ( raw == 0xF0 )                              
        {
            raw = GetNextESKeyboardRawcode();                               // try to get the one that's breaking
            
            if ( raw == 0xFF )                                              // not in buf yet?
            {
                BackUpKeycodeTail();
                return( kNoKey );
            }
                
            if ( raw < scancodeTableSize )       // valid code?
            {
                map = scancodeToAscii[raw];
                
                if ( map == kShiftKey )             
                    ControlGlobalz.keyboardFlags &= ~kShiftDown;

                if ( map == kAltKey )               
                    ControlGlobalz.keyboardFlags &= ~kAltDown;

                if ( map == kControlKey )               
                    ControlGlobalz.keyboardFlags &= ~kControlDown;

                if ( map == kCapsLockKey )          
                    ControlGlobalz.keyboardFlags &= ~kCapsLockDown;             // caps lock up
            }
            
            return( kNoKey );
        }
        
        //
        // 101-STYLE CODES
        //

        if ( raw == 0xE0 )                                                  
        {
            raw = GetNextESKeyboardRawcode();                       // get key code
            
            if ( raw == 0xFF )                                      // special key there yet?
            {
                BackUpKeycodeTail();
                return( kNoKey );
            }
            else                                                    // special key *is* there
            {
                if ( raw == 0xF0 )                                  // just trash breaks
                {
                    raw = GetNextESKeyboardRawcode();               // try to get one to break
                    if ( raw == 0xFF )                              // key to break here?
                    {
                        BackUpKeycodeTail();
                        BackUpKeycodeTail();
                    }
                    return( kNoKey );
                }
                specialPending = 1;              // flag it special
            }
        }
        
        //
        // NORMAL CODES
        //
        
        if( raw < scancodeTableSize )                                              // valid code?
        {
            map = scancodeToAscii[raw];                                            // map to lower case

            if( ControlGlobalz.keyboardFlags & kShiftDown )                        // shift key -> ALL shifted
            {
                if ( (map >= 0x20) && (map <= 0x7E) )
                {
                    map = scancodeToAsciiShifted[raw];                              // map to shifted
                }   
            }
            else
                if( ControlGlobalz.keyboardFlags & kCapsLocked )                    // capslock key -> CHARS shifted
                {
                    if ( (map >= 'a') && (map <= 'z') )
                    {
                        map = scancodeToAsciiShifted[raw];                          // map to shifted
                    }
                }
        }
        
        //
        // META KEYS
        //
            
        switch(map)
        {   
            case kShiftKey:
                    ControlGlobalz.keyboardFlags |= kShiftDown;
                    return( kNoKey );
                    break;
        
            case kAltKey:
                    ControlGlobalz.keyboardFlags |= kAltDown;
                    return( kNoKey );
                    break;
        
            case kControlKey:
                    ControlGlobalz.keyboardFlags |= kControlDown;
                    return( kNoKey );
                    break;
        }
        
        if ( (map == kCapsLockKey) && !(ControlGlobalz.keyboardFlags & kCapsLockDown) )
        {
            ControlGlobalz.keyboardFlags |= kCapsLockDown;    
            ControlGlobalz.keyboardFlags ^= kCapsLocked;                    // flip caps lock state
            fuck[0] = 0xED;                                                 // hit the LED reg
            fuck[1] = ControlGlobalz.keyboardFlags & kCapsLocked;           // bits [2:0] are caps/num/scroll lock
            SendCmdToESKeyboard( fuck, 2 );
            return( kNoKey );
        }

        //
        // Special Keys
        //

        
        // Let's leave it as 0x0A
        
        // if( map == 0x0A )
        //     map = kEnterKey;
    

        if ( specialPending )
        {
            switch( raw )
            {
                case 0x75:                          // up arrow
                            map = kUpArrowKey;
                            break;
                case 0x72:                          // down arrow
                            map = kDownArrowKey;
                            break;
                case 0x6B:                          // left arrow
                            map = kLeftArrowKey;
                            break;
                case 0x74:                          // right arrow
                            map = kRightArrowKey;
                            break;
                case 0x5A:
                            map = kEnterKey;
                            break;
                default:
                            map = kNoKey;
                            break;
            }
        }   

    }
        
    return( map );
}



unsigned char GetNextESKeyboardRawcode( void )
{
    if( ControlGlobalz.keycodeTail != ControlGlobalz.keycodeHead )
    {
        ControlGlobalz.keycodeTail ++;
        ControlGlobalz.keycodeTail &= kKeybdDataFifoMask;
        return ControlGlobalz.keycodeBuf[ControlGlobalz.keycodeTail];
    }
    else
        return( 0xFF );
}



void WaitForVBlank() {
	
	volatile unsigned short *pw;

    pw = (volatile unsigned short *) 0xC00004;

    while (*pw & (1 << 3));
	while (!(*pw & (1 << 3)));	
}


