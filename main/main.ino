
#include <ArduCAM.h> //Setting up the camera library
#include <Wire.h> 
#include <SPI.h> //Setting up the SPI comms
#include <SD.h> //Setting up the SD card logic/communication
#include <ezButton.h> //Setting the limit switch


//Setting up the SD card chipselect
#define SD_CS 9 

//Setting up the ARDUCAM
const int camera_CS =10;
ArduCAM myCAM( OV5642, camera_CS );


//Setting up the limit switch
ezButton limitSwitch(7);


void takePicture(){
  char str[8];
  byte buf[256];
  static int i = 0,k = 0;
  uint8_t temp = 0,temp_last = 0;
  uint32_t length = 0;
  bool is_header = false;
  File outFile;

  //Flush FIFO
  myCAM.flush_fifo();
  //Clear the capture done flag
  myCAM.clear_fifo_flag();
  //Start the capture
  myCAM.start_capture();
  Serial.println("Starting Capture.");
  while(!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
  Serial.println("Capture Done.");
  length = myCAM.read_fifo_length();
  Serial.print("The fifo length is: ");
  Serial.println(length,DEC);

  if (length >= MAX_FIFO_SIZE) {
    Serial.println("Over Size.");
    return;
  }
  if (length == 0){
    Serial.println("Size is 0.");
    return;
  }

  //Construct a outFile name
  k = k + 1;
  itoa(k, str, 10);
  strcat(str, ".jpg");
  //Open the new outFile
  outFile = SD.open(str, FILE_WRITE);
  if(! outFile){
    Serial.println(F("File open failed"));
    return;
  }

  i = 0;
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();

  //Read JPEG data from FIFO
  while ( length-- ) {
    temp_last = temp;
    temp =  SPI.transfer(0x00);
    //Read JPEG data from FIFO
    if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
    {
      buf[i++] = temp;  //save the last  0XD9     
      //Write the remain bytes in the buffer
      myCAM.CS_HIGH();
      outFile.write(buf, i);    
      //Close the file
      outFile.close();
      Serial.println(F("Image Save Done!"));
      is_header = false;
      i = 0;
    }  
    if (is_header == true)
    { 
      //Write image data to buffer if not full
      if (i < 256)
        buf[i++] = temp;
      else
      {
        //Write 256 bytes image data to file
       myCAM.CS_HIGH();
       outFile.write(buf, 256);
       i = 0;
       buf[i++] = temp;
        myCAM.CS_LOW();
        myCAM.set_fifo_burst();
      }        
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
      is_header = true;
      buf[i++] = temp_last;
      buf[i++] = temp;   
    } 
  }

  delay(2000);
  Serial.println("Ready for Next Photo");

}








void setup () {
  //These are for checking in on SPI and SD card initializations
  uint8_t vid = 0,pid = 0;
  uint8_t temp = 0;
  Wire.begin();

  Serial.begin(115200);

  //Turn on the camera
  pinMode(camera_CS,OUTPUT);
  SPI.begin();
  
  //Set camera to low
  myCAM.CS_LOW();

  // Re-enable the memory controller circuit
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);
  
  //Check on the ARDUCAM SPI Response
  while(1){
    //Check if the ArduCAM SPI bus is OK
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    delay(100);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if(temp != 0x55)
    {
      Serial.println(F("SPI interface Error!, Trying Again"));
      delay(1000);
    }else{
      Serial.println(F("SPI interface OK."));break;
    }
  }

  //Initialize SD Card
  while(!SD.begin(SD_CS)){
    Serial.println(F("SD Card Error! Trying Again"));delay(1000);
  }
  Serial.println(F("SD Card detected."));

  while(1){
    //Check if the camera module type is OV5642
    myCAM.wrSensorReg16_8(0xff, 0x01);
    myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
    if((vid != 0x56) || (pid != 0x42)){
      Serial.println(F("Can't find OV5642 module! Trying Again"));
      delay(1000);
    } else{
     Serial.println(F("OV5642 detected.")); break;
    }
  }


  //Other parameter settings for the camera
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
  myCAM.OV5642_set_JPEG_size(OV5642_2592x1944);delay(1000);
  
  
  delay(1000);

}


void loop () {
  limitSwitch.loop();
  if(limitSwitch.isPressed()) {
    takePicture();
  }                                                                        
}