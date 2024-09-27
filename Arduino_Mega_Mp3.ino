/*************************************************** 
  This is my ugly mp3 player using these components
   - Arduino Mega 
   - Adafruit VS1053 MP3 Music play shield
   - lcd1604_i2c 20x4 lcd display
   - 4x4 Keypad 

  Use 4x4 keypad to select 3 digit track # then hit * key to start playing
  By default starts playing songs from track000.mp3 to track999.mp3 
  Press the D key to use the shuffled songs order or press D key to return to sequential order
  
  Key Mapping: 
   A = Next Song
   B = Previous Song
   C = Pause present playing song and Resume Playing
   D = Toggle sequential Song Order or Shuffle
   * = Enter key of 3 digit song selection
   # = Not Mapped or used yet
  
  SD card stores file names uppercase 8.3 filename (ie TRACK00x.MP3)
  My music player extracts the mp3 metadata from file of Title & Artist
  
  Prior to copying mp3 files to micro SD card I use the "EasyTag" app to edit the mp3 meta data.
  This allows me to insure that the Title and Artist information is present to display to lcd
  I renamed my music filename from original_mp3_filename.mp3 to tracknnn.mp3 (track000.mp3 to track999.mp3) 
  then copy the renamed tracknnn.mp3 files to the root folder on the micro sd card.
  Last Update: Sept 10, 2024
 ****************************************************/

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,20,4); 

#include <DIYables_Keypad.h>  

const int ROW_NUM = 4;     //four rows
const int COLUMN_NUM = 4;  //four columns

char keys[ROW_NUM][COLUMN_NUM] = {
  { '1', '4', '7', '*' },
  { '2', '5', '8', '0' },
  { '3', '6', '9', '#' },
  { 'A', 'B', 'C', 'D' }
};

byte pin_rows[ROW_NUM] = { 26, 27, 28, 29 };       //connect to the row pinouts of the keypad
byte pin_column[COLUMN_NUM] = { 30, 31, 32, 33 };  //connect to the column pinouts of the keypad

DIYables_Keypad keypad = DIYables_Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);


// These are the pins used for the breakout example
#define BREAKOUT_RESET  9      // VS1053 reset pin (output)
#define BREAKOUT_CS     10     // VS1053 chip select pin (output)
#define BREAKOUT_DCS    8      // VS1053 Data/command select pin (output)
// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

// Reminder Mega can store array of 32,767 bytes the file name array should reflect count of songs on sd  
#define MaxInArray 100 // Was 1000
#define max_title_len 60
#define max_artist_len 30
#define max_name_len 13

char *fileName[MaxInArray];
char tempString[13];
char artist[max_artist_len + 1];
char title[max_title_len + 1];
int shuffleSort[MaxInArray];
bool ShuffleSongs = false;

int t = 0;
int tmp_t = 0;
int mydot = 0;
bool mydirection = true; 
bool pauseflag = false;
int numberElementsInArray = 0;
String mystring;
int i;
int v = 5; // Starting volumne Lower # increases volumne


//Identifiers
boolean isPlaying = false;
boolean autoPlay = false;

void setup() {
  Serial.begin(115200);
  lcd.init();                      // initialize the lcd 
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(3,0);
  lcd.print("MP3 Player");
  lcd.setCursor(2,1);
  lcd.print("Ywrobot Arduino!");
  lcd.setCursor(0,2);
  lcd.print("Arduino LCM IIC 2004");
  lcd.setCursor(2,3);
  lcd.print("Power By Ec-yuan!");  
  
  Serial.println("Adafruit VS1053 MP3Player");
  
  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  Serial.println(F("VS1053 found"));

  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
 
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  
  // list files
  printDirectory(SD.open("/"), 0); // populates fileName[] with root folder mp3 files and numberElementsInArray from sd card
  // Init shuffle songs array (Init to be 0, 1, 2, x)
  for (int i = 0; i< numberElementsInArray; i++) {
    shuffleSort[i] = i;
  }
  buildshuffleSort(); // Now shuffle the order of the shuffle array
  sortFileArray(); // Sorts fileName array for sequential song play order
  printArray();  // Lists the contents fileName array (mp3 files on sd card
  helpmsg(); 
 
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(v,v);

  /***** Two interrupt options! *******/ 
  // This option uses timer0, this means timer1 & t2 are not required
  // (so you can use 'em for Servos, etc) BUT millis() can lose time
  // since we're hitchhiking on top of the millis() tracker
  //musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT);
  
  // This option uses a pin interrupt. No timers required! But DREQ
  // must be on an interrupt pin. For Uno/Duemilanove/Diecimilla
  // that's Digital #2 or #3
  // See http://arduino.cc/en/Reference/attachInterrupt for other pins
  // *** This method is preferred
  if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT))
    Serial.println(F("DREQ pin is not an interrupt pin"));
}

void loop() {  

  // Start playing a file, then we can do stuff while waiting for it to finish
  if (!ShuffleSongs) {
    // Use the sequential file order
    strcpy(tempString,fileName[t]);
  } else {
    // Use the shuffleSorted order
    strcpy(tempString,fileName[shuffleSort[t]]);
  }
  
  mydot = 0;
  mydirection = true;

  get_title_from_id3tag();
  get_artist_from_id3tag();
  Serial.println(String(title) + " - " + String(artist));
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(String(title));  
  lcd.setCursor(0,2);
  lcd.print(String(artist)); 
  lcd.setCursor(0,3);
  lcd.print(String(tempString));      
  
  if (! musicPlayer.startPlayingFile(tempString)) {
    Serial.println("Could not open file [" + String(tempString) + "]");
    while (1);
  }
  
  Serial.println("Playing track [" + String(t) + "] ... [" + String(tempString) + "]");

  while (musicPlayer.playingMusic or musicPlayer.paused()) {
    // file is now playing in the 'background' so now's a good time
    // to do something else like handling LEDs or buttons :)
    
    char key = keypad.getKey();

    if (key) {
          if (key >= '0' || key <= '9') {
            mystring = mystring + String(key);
            // If index is larger than total in array wipe it and start over
            if (mystring.toInt() > numberElementsInArray ) {
              mystring="";
            }
            Serial.println(mystring);
          }
    }

    // Toggle Sort order (Sequential/Shuffle)
    if (key == '#') {
      if (ShuffleSongs) {
        ShuffleSongs = false;
      }else {
        ShuffleSongs = true;
      
      }
    }

    // User choose track number to play
    if (key == '*') {
      tmp_t=mystring.toInt();
      mystring="";
      musicPlayer.stopPlaying();
      t=tmp_t; 
      // Last song so loop back to first song
      if (t > numberElementsInArray ) {
        t = 0;
      }
      strcpy(tempString,fileName[t]);
      get_title_from_id3tag();
      get_artist_from_id3tag();
      Serial.println(String(title) + " - " + String(artist));
      mydot=0;
      mydirection=true;
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print(String(title));  
      lcd.setCursor(0,2);
      lcd.print(String(artist)); 
      lcd.setCursor(0,3);
      lcd.print(String(tempString));         
      Serial.println("Playing next track [" + String(t) + "] ... [" + String(tempString) + "]");
      musicPlayer.startPlayingFile(tempString);
    } // End *

    
    // Next Button
    if (key == 'A') {
      musicPlayer.stopPlaying();
      t++; 
      mystring="";
      // Last song so loop back to first song
      if (t > numberElementsInArray ) {
        t = 0;
      }
      if (!ShuffleSongs) {
        // Use the sequential file order
        strcpy(tempString,fileName[t]);
      } else {
        // Use the shuffleSorted order
        strcpy(tempString,fileName[shuffleSort[t]]);
      }
      get_title_from_id3tag();
      get_artist_from_id3tag();
      Serial.println(String(title) + " - " + String(artist));
      mydot=0;
      mydirection=true;
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print(String(title));  
      lcd.setCursor(0,2);
      lcd.print(String(artist)); 
      lcd.setCursor(0,3);
      lcd.print(String(tempString));         
      Serial.println("Playing next track [" + String(t) + "] ... [" + String(tempString) + "]");
      musicPlayer.startPlayingFile(tempString);          
    } // end next 

    // Prev Button
    if (key == 'B') {
      musicPlayer.stopPlaying();
      t--; 
      mystring="";
      // Cant go back so play first song
      if (t <= 0 ) {
        t = 0;
      }
        
      if (!ShuffleSongs) {
        // Use the sequential file order
        strcpy(tempString,fileName[t]);
      } else {
        // Use the shuffleSorted order
        strcpy(tempString,fileName[shuffleSort[t]]);
      }
      
      get_title_from_id3tag();
      get_artist_from_id3tag();
      Serial.println(String(title) + " - " + String(artist));
      mydot=0;
      mydirection=true;        
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print(String(title));  
      lcd.setCursor(0,2);
      lcd.print(String(artist)); 
      lcd.setCursor(0,3);
      lcd.print(String(tempString));         
      Serial.println("Playing prev track [" + String(t) + "] ... [" + String(tempString) + "]");
      musicPlayer.startPlayingFile(tempString);  
    } // end prev
    
    // if we get an 'C' pause/unpause!
    if (key == 'C') {
      Serial.println();
      if (! musicPlayer.paused()) {
        Serial.println("Paused (Press P to resume playing)");
        musicPlayer.pausePlaying(true);
        pauseflag = true;
        lcd.setCursor(0,0);
        lcd.print("<P A U S E  P L A Y>");
      } else { 
        Serial.println("Resumed");
        musicPlayer.pausePlaying(false);
        pauseflag =false;
      } // End musicPlayer.Pause
    } // end P Pressed
    
    if (Serial.available()) {
      char c = Serial.read();

      // if we get an '?' help
      if (c == '?') {
        helpmsg();
      } // help
    
      // if we get an 'a' increase volume
      if (c == 'a') {
        Serial.println();
        v = v - 2;
        if (v < 1) {
          v = 1;
        }
        musicPlayer.setVolume(v,v);
        Serial.println("Increase Volume [" +String(v) + "]");
      } // Increase volume

      // if we get an 'z' decrease volume
      if (c == 'z') {
        Serial.println();
        v = v + 2;
        if (v > 60) {
          v = 60;
        }
        musicPlayer.setVolume(v,v);
        Serial.println("Decrease Volume [" +String(v) + "]");
      } // Increase volume      
      
      // if we get an 's' on the serial console, stop!
      if (c == 's') {
        musicPlayer.stopPlaying();
        Serial.println("Stop playing music");
        t = 0;
      } // End stop playing      
      
      // if we get an 'n' on the serial console, stop, next!
      if (c == 'n') {
        musicPlayer.stopPlaying();
        t++; 
        // Last song so loop back to first song
        if (t > numberElementsInArray ) {
          t = 0;
        }
        strcpy(tempString,fileName[t]);
        get_title_from_id3tag();
        get_artist_from_id3tag();
        Serial.println(String(title) + " - " + String(artist));
        mydot=0;
        mydirection=true;
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print(String(title));  
        lcd.setCursor(0,2);
        lcd.print(String(artist)); 
        lcd.setCursor(0,3);
        lcd.print(String(tempString));         

        Serial.println("Playing next track [" + String(t) + "] ... [" + String(tempString) + "]");
        musicPlayer.startPlayingFile(tempString);          
      } // end next     

      // if we get an 'l' play last song
      if (c == 'l') {
        musicPlayer.stopPlaying();
        t = numberElementsInArray - 1; 
        // Last song so loop back to first song
        if (t > numberElementsInArray ) {
          t = 0;
        }
        strcpy(tempString,fileName[t]);
        get_title_from_id3tag();
        get_artist_from_id3tag();
        Serial.println(String(title) + " - " + String(artist));
        mydot=0;
        mydirection=true;
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print(String(title));  
        lcd.setCursor(0,2);
        lcd.print(String(artist)); 
        lcd.setCursor(0,3);
        lcd.print(String(tempString));         

        Serial.println("Playing last track [" + String(t) + "] ... [" + String(tempString) + "]");
        musicPlayer.startPlayingFile(tempString);          
      } // end last      

      // if we get an 'f' play first song
      if (c == 'f') {
        musicPlayer.stopPlaying();
        t = 0; 
        // Last song so loop back to first song
        if (t > numberElementsInArray ) {
          t = 0;
        }
        strcpy(tempString,fileName[t]);
        get_title_from_id3tag();
        get_artist_from_id3tag();
        Serial.println(String(title) + " - " + String(artist));
        mydot=0;
        mydirection=true;        
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print(String(title));  
        lcd.setCursor(0,2);
        lcd.print(String(artist)); 
        lcd.setCursor(0,3);
        lcd.print(String(tempString));         

        Serial.println("Playing first track [" + String(t) + "] ... [" + String(tempString) + "]");
        musicPlayer.startPlayingFile(tempString);          
      } // end next                 

      // if we get an 'p' on the serial console, stop, prev!
      if (c == 'p') {
        musicPlayer.stopPlaying();
        t--; 
        // Cant go back so play first song
        if (t <= 0 ) {
          t = 0;
        }
        
        strcpy(tempString,fileName[t]);
        get_title_from_id3tag();
        get_artist_from_id3tag();
        Serial.println(String(title) + " - " + String(artist));
        mydot=0;
        mydirection=true;        
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print(String(title));  
        lcd.setCursor(0,2);
        lcd.print(String(artist)); 
        lcd.setCursor(0,3);
        lcd.print(String(tempString));         

        Serial.println("Playing prev track [" + String(t) + "] ... [" + String(tempString) + "]");
        musicPlayer.startPlayingFile(tempString);  
      } // end prev     
        

    
    } // end Serial.Avail    
    
    //Serial.print(".");
    if (mydirection == true and pauseflag == false) {
      mydot++;
      if (mydot > 17) {
        mydirection = false;
      }
      lcd.setCursor(mydot,0);
      lcd.print("->");
    }
    if (mydirection == false and pauseflag == false) {
      mydot--;
      if (mydot == 0) {
        mydirection = true;
      }
      lcd.setCursor(mydot,0);
      lcd.print("<-");
    }
    delay(10); // Get rid of delay and use milli() counter
  } // end while
  
  t++;
  if ( t > numberElementsInArray ) {
    t = 0;
  }  
  Serial.println("Done playing music");
} // end loop

void buildshuffleSort() {
  // Lets shuffle randomly 2 - 5 times
  for (int myi = 0; i < random(2,5); myi++) {
    for (int i = 0; i < numberElementsInArray; i++){
      int j = random(0, numberElementsInArray - i);

      int t = shuffleSort[i];
      shuffleSort[i] = shuffleSort[j];
      shuffleSort[j] = t;
    }  
  }
}

void freeMessageMemory()
{
  // If we have previous messages, then free the memory
      for (byte i=1; i<=numberElementsInArray; i++)
        {
          free(fileName[i-1]);
        }
        
    numberElementsInArray = 0;
  
}

void sortFileArray()
{
  
  int innerLoop ;
  int mainLoop ;

  for ( mainLoop = 1; mainLoop < numberElementsInArray; mainLoop++)
    {
      innerLoop = mainLoop;
      while (innerLoop  >= 1)
          {
          if (arrayLessThan(fileName[innerLoop], fileName[innerLoop-1]) == 1)
            {            
              switchArray(innerLoop);
            }
            innerLoop--;
          }
    }
}

byte arrayLessThan(char *ptr_1, char *ptr_2)
{
  char check1;
  char check2;
  
  int i = 0;
  while (i < strlen(ptr_1))    // For each character in string 1, starting with the first:
    {
        check1 = (char)ptr_1[i];  // get the same char from string 1 and string 2
        
        //Serial.print("Check 1 is "); Serial.print(check1);
          
        if (strlen(ptr_2) < i)    // If string 2 is shorter, then switch them
            {
              return 1;
            }
        else
            {
              check2 = (char)ptr_2[i];
           //   Serial.print("Check 2 is "); Serial.println(check2);

              if (check2 > check1)
                {
                  return 1;       // String 2 is greater; so switch them
                }
              if (check2 < check1)
                {
                  return 0;       // String 2 is LESS; so DONT switch them
                }
               // OTHERWISE they're equal so far; check the next char !!
             i++; 
            }
    }
    
return 0;  
}

void switchArray(byte value)
{
 // switch pointers i and i-1, using a temp pointer. 
 char *tempPointer;

 tempPointer = fileName[value-1];
 fileName[value-1] = fileName[value];
 fileName[value] = tempPointer;
}

void printArray()
{
  Serial.println("The array currently holds :");
  for (i = 0; i< numberElementsInArray; i++)
    {
      Serial.println(fileName[i]);
    }
}

void helpmsg() {
  Serial.println("n=Next,p=Prev,f=First,l=Last,s=Stop,P=Pause,a=+Vol,z=-Vol,?=Help");
}
/// File listing helper
void printDirectory(File dir, int numTabs) {
   String myfile;
   freeMessageMemory();
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     // Count only files ending in mp3
     myfile = entry.name();
     if (myfile.endsWith(".MP3")) {
      strcpy(tempString,entry.name());
      fileName[numberElementsInArray] = (char *)malloc(13);
      strcpy(fileName[numberElementsInArray], tempString);
      numberElementsInArray++;     
     }
     
     //if (entry.isDirectory()) {
     //  Serial.println("/");
     //  printDirectory(entry, numTabs+1);
     //} 
     entry.close();
   }
   Serial.println("Found [" + String(numberElementsInArray) + "] mp3 files");
}

void get_title_from_id3tag () {
  unsigned char id3[3];       // pointer to the first 3 characters to read in

  // visit http://www.id3.org/id3v2.3.0 to learn all(!) about the id3v2 spec.
  // move the file pointer to the beginning, and read the first 3 characters.
  File sd_file;
  sd_file = SD.open(tempString, FILE_READ);
 
  sd_file.seek(0);
  sd_file.read(id3, 3);
  
  // if these first 3 characters are 'ID3', then we have an id3v2 tag. if so,
  // a 'TIT2' (for ver2.3) or 'TT2' (for ver2.2) frame holds the song title.
  // a 'TPE1' Gives Artist
  if (strncmp(id3,"ID3",3) == 0) {
    unsigned char pb[4];       // pointer to the last 4 characters we read in
    unsigned char c;           // the next 1 character in the file to be read
    
    // our first task is to find the length of the (whole) id3v2 tag. knowing
    // this means that we can look for 'TIT2' or 'TT2' frames only within the
    // tag's length, rather than the entire file (which can take a while).

    // skip 3 bytes (that we don't use), then read in the last 4 bytes of the
    // header, which contain the tag's length.

    sd_file.read(pb, 3);
    sd_file.read(pb, 4);
    
    // to combine these 4 bytes together into the single value, we first have
    // to shift each one over to get it into its correct 'digits' position. a
    // quirk of the spec is that bit 7 (the msb) of each byte is set to 0.
    
    unsigned long v2l = ((unsigned long) pb[0] << (7 * 3)) +
                        ((unsigned long) pb[1] << (7 * 2)) +
                        ((unsigned long) pb[2] << (7 * 1)) + pb[3];
                        
    // we just moved the file pointer 10 bytes into the file, so we reset it.
    
    sd_file.seek(0);

    while (1) {
      // read in bytes of the file, one by one, so we can check for the tags.
      
      sd_file.read(&c, 1);

      // keep shifting over previously-read bytes as we read in each new one.
      // that way we keep testing if we've found a 'TIT2' or 'TT2' frame yet.
      
      pb[0] = pb[1];
      pb[1] = pb[2];
      pb[2] = pb[3];
      pb[3] = c;

      
      if (strncmp(pb,"TIT2",4) == 0) {
        // found an id3v2.3 frame! the title's length is in the next 4 bytes.
        
        sd_file.read(pb, 4);

        // only the last of these bytes is likely needed, as it can represent
        // titles up to 255 characters. but to combine these 4 bytes together
        // into the single value, we first have to shift each one over to get
        // it into its correct 'digits' position. 

        unsigned long tl = ((unsigned long) pb[0] << (8 * 3)) +
                           ((unsigned long) pb[1] << (8 * 2)) +
                           ((unsigned long) pb[2] << (8 * 1)) + pb[3];
        tl--;
        
        // skip 2 bytes (we don't use), then read in 1 byte of text encoding. 

        sd_file.read(pb, 2);
        sd_file.read(&c, 1);
        
        // if c=1, the title is in unicode, which uses 2 bytes per character.
        // skip the next 2 bytes (the byte order mark) and decrement tl by 2.
        
        if (c) {
          sd_file.read(pb, 2);
          tl -= 2;
        }
        // remember that titles are limited to only max_title_len bytes long.
        
        if (tl > max_title_len) tl = max_title_len;
        
        // read in tl bytes of the title itself. add an 'end-of-string' byte.

        sd_file.read(title, tl);
        title[tl] = '\0';
        break;
      } // end TIT2
      else
      if (pb[1] == 'T' && pb[2] == 'T' && pb[3] == '2') {
        // found an id3v2.2 frame! the title's length is in the next 3 bytes,
        // but we read in 4 then ignore the last, which is the text encoding.
        
        sd_file.read(pb, 4);
        
        // shift each byte over to get it into its correct 'digits' position. 
        
        unsigned long tl = ((unsigned long) pb[0] << (8 * 2)) +
                           ((unsigned long) pb[1] << (8 * 1)) + pb[2];
        tl--;
        
        // remember that titles are limited to only max_title_len bytes long.

        if (tl > max_title_len) tl = max_title_len;

        // there's no text encoding, so read in tl bytes of the title itself.
        
        sd_file.read(title, tl);
        title[tl] = '\0';
        break;
      } // end TT2
      else
      if (sd_file.position() == v2l) {
        // we reached the end of the id3v2 tag.

        title[0]='\0';
        break;
      }
    } // end while (1)
  }
  else {
      // there is no id3 tag or the id3 version is not supported.
      title[0]='\0';
  } 
}

void get_artist_from_id3tag () {
  unsigned char id3[3];       // pointer to the first 3 characters to read in

  // visit http://www.id3.org/id3v2.3.0 to learn all(!) about the id3v2 spec.
  // move the file pointer to the beginning, and read the first 3 characters.
  File sd_file;
  sd_file = SD.open(tempString, FILE_READ);
    
  sd_file.seek(0);
  sd_file.read(id3, 3);
  
  // if these first 3 characters are 'ID3', then we have an id3v2 tag. if so,
  // a 'TPE1' Gives Artist
  if (strncmp(id3,"ID3",3) == 0) {
    unsigned char pb[4];       // pointer to the last 4 characters we read in
    unsigned char c;           // the next 1 character in the file to be read
    
    // our first task is to find the length of the (whole) id3v2 tag. knowing
    // this means that we can look for 'TIT2' or 'TT2' frames only within the
    // tag's length, rather than the entire file (which can take a while).

    // skip 3 bytes (that we don't use), then read in the last 4 bytes of the
    // header, which contain the tag's length.

    sd_file.read(pb, 3);
    sd_file.read(pb, 4);
    
    // to combine these 4 bytes together into the single value, we first have
    // to shift each one over to get it into its correct 'digits' position. a
    // quirk of the spec is that bit 7 (the msb) of each byte is set to 0.
    
    unsigned long v2l = ((unsigned long) pb[0] << (7 * 3)) +
                        ((unsigned long) pb[1] << (7 * 2)) +
                        ((unsigned long) pb[2] << (7 * 1)) + pb[3];
                        
    // we just moved the file pointer 10 bytes into the file, so we reset it.
    
    sd_file.seek(0);

    while (1) {
      // read in bytes of the file, one by one, so we can check for the tags.
      
      sd_file.read(&c, 1);

      // keep shifting over previously-read bytes as we read in each new one.
      // that way we keep testing if we've found a 'TIT2' or 'TT2' frame yet.
      
      pb[0] = pb[1];
      pb[1] = pb[2];
      pb[2] = pb[3];
      pb[3] = c;

      
      if (strncmp(pb,"TPE1",4) == 0) {
        // found an id3v2.3 frame! the title's length is in the next 4 bytes.
        sd_file.read(pb, 4);

        // only the last of these bytes is likely needed, as it can represent
        // titles up to 255 characters. but to combine these 4 bytes together
        // into the single value, we first have to shift each one over to get
        // it into its correct 'digits' position. 

        unsigned long tl = ((unsigned long) pb[0] << (8 * 3)) +
                           ((unsigned long) pb[1] << (8 * 2)) +
                           ((unsigned long) pb[2] << (8 * 1)) + pb[3];
        tl--;
        
        // skip 2 bytes (we don't use), then read in 1 byte of text encoding. 

        sd_file.read(pb, 2);
        sd_file.read(&c, 1);
        
        // if c=1, the title is in unicode, which uses 2 bytes per character.
        // skip the next 2 bytes (the byte order mark) and decrement tl by 2.
        
        if (c) {
          sd_file.read(pb, 2);
          tl -= 2;
        }
        // remember that titles are limited to only max_title_len bytes long.
        
        if (tl > max_artist_len ) tl = max_artist_len;
        
        // read in tl bytes of the title itself. add an 'end-of-string' byte.

        sd_file.read(artist, tl);
        artist[tl] = '\0';
        break;
      } // end TPE1
      else
      if (sd_file.position() == v2l) {
        // we reached the end of the id3v2 tag. No artist found
        artist[0]='\0';
        break;
      }
    } // end while (1)
  }
  else {
      // there is no id3 tag or the id3 version is not supported.
      artist[0]='\0';
  } 
}
