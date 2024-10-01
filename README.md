  This is my ugly mp3 player using these components
   - Arduino Mega 
   - Adafruit VS1053 MP3 Music play shield
   - lcd1604_i2c 20x4 lcd display
   - 4x4 Keypad 

  Reads the mp3 meta-data to display the artist and song on the LCD
  
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
