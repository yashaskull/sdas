  #include "Protocentral_ADS1220.h"
//#include "SPI.h"

#define PGA 1
#define VREF1 5
#define VREF2 2.048

#define VFSR1 VREF1/PGA
#define VFSR2 VREF2/PGA
#define FSR (((long int)1<<23)-1)

//#define PACKET_COUNT 100
//#define BLOCK_COUNT 1187
// how often to update time
//#define MAX_BLOCK_RECORD (PACKET_COUNT*BLOCK_COUNT)// 1200 record ~ 20.13 minutes, try 1187 for 20.000 minutes

#define TIMESTAMP_PIN 32

///////////////////
float xSensitivity = 0.32807; //0.32780;
float xZeroG = 1.66527; //1.667;

float ySensitivity = 0.33441; //0.33420;i
float yZeroG = 1.66364; //1.66400;

float zSensitivity = 0.32925; //0.32905;
float zZeroG = 1.70094; //1.69886;
////////////////////

volatile byte MSB;
volatile byte data;
volatile byte LSB;
volatile byte *SPI_RX_Buff_Ptr_1;
volatile byte *SPI_RX_Buff_Ptr_2;
volatile byte *SPI_RX_Buff_Ptr_3;

int bit16;
long int bit24;
long int bit24_E;
long int bit24_N;
long int bit24_Z;

long int bit24_E_temp = 0;
long int bit24_N_temp = 0;
long int bit24_Z_temp = 0;
int avg_count = 0;

float temp;
float Vout;
float Vout_N;
float Vout_E;
float Vout_Z;
long int dataZ;
long int dataNS;
long int dataPot;
long int dataEW;
//float start;
//float  endtim
Protocentral_ADS1220 ADS1220;
// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
// Volatile variables can change inside interrupts
// Whenever a varible will be changing inside and interrupt,
// it must be declared a volatile


int logicPin = 0;
int leave = 0;
int ignoreFirst100Samp = 0;
unsigned long starttime = 0;
unsigned long starttime_temp = 0;
unsigned long endtime = 0;
volatile int startCommand;
int blockLength_req = 0;
int stop_check = 0;
/////////////////////////
long int maxBlockRecord;
long int packetCount = 100;
long int blockCount = 1187;// 1187
long int sampleCount = 0;
//////////////////////////

int skip = 0;

String serialData;
bool print2Serial = false;

volatile int stopPinChange = 0;
void printData();
void print_voltage();
volatile int counter = 0;
const byte interruptPin = 3;

int sample_counter = 0;
////////////////////

////////////////////

void setup() {

  Serial.begin(115200);

  
  while (!Serial.available())
  {
    delay(1000);
     //do nothing
  }
  
  while(Serial.available())
  {
    Serial.read();
  }
  delay(1000);
  
  pinMode(ADS1220_CS_PIN_1, OUTPUT);
  digitalWrite(ADS1220_CS_PIN_1, HIGH);
  pinMode(ADS1220_DRDY_PIN_1, INPUT);
  delay(5);

  pinMode(ADS1220_CS_PIN_2, OUTPUT);
  digitalWrite(ADS1220_CS_PIN_2, HIGH);
  pinMode(ADS1220_DRDY_PIN_2, INPUT);
  delay(5);

  pinMode(ADS1220_CS_PIN_3, OUTPUT);
  digitalWrite(ADS1220_CS_PIN_3, HIGH);
  pinMode(ADS1220_DRDY_PIN_3, INPUT);
  delay(5);

  // need to set pin 53 to OUTPUT for SPI communication
  pinMode(53, OUTPUT);// need to change

  pinMode(22, OUTPUT);
  // count down timer and digital pin interrupt timer
  ////////////////////////////////////
  //setupTimer();
  //pinMode(interruptPin, INPUT);
  //attachInterrupt(digitalPinToInterrupt(interruptPin), printCounter, RISING);
  ////////////////////////////////////


  
ADS1220.begin();
 // ADS1220.powerDown();
    //digitalWrite(22, HIGH);

  //pinsInput();/////////////////////////////////////////////////
  Serial.println("*");
  //setupTimer();
 // starttime = micros();
}
void loop()
{
  if (digitalRead(ADS1220_DRDY_PIN_1) == LOW && digitalRead(ADS1220_DRDY_PIN_2) == LOW  && digitalRead(ADS1220_DRDY_PIN_3) == LOW)
  {
  
    if (sample_counter == 2)// 200 SPS
      readData(true);
    else
    {
      readData(false);
      sample_counter++;
    }
    //readData(true);
  }
  
  if (counter == 200)
  {
    Serial.println("*");//////////////////////////////////////////
    counter = 0;
  }
  
}// end loop

void readData(bool print2Serial)
{  
  SPI_RX_Buff_Ptr_1 = ADS1220.Read_Data_1();
  SPI_RX_Buff_Ptr_2 = ADS1220.Read_Data_2();
  SPI_RX_Buff_Ptr_3 = ADS1220.Read_Data_3();

  MSB = SPI_RX_Buff_Ptr_1[0];
  data = SPI_RX_Buff_Ptr_1[1];
  LSB = SPI_RX_Buff_Ptr_1[2];
  bit24_N = MSB;
  bit24_N = (bit24_N << 8) | data;
  bit24_N = (bit24_N << 8) | LSB;

  MSB = SPI_RX_Buff_Ptr_2[0];
  data = SPI_RX_Buff_Ptr_2[1];
  LSB = SPI_RX_Buff_Ptr_2[2];
  bit24_E = MSB;
  bit24_E = (bit24_E << 8) | data;
  bit24_E = (bit24_E << 8) | LSB;

  MSB = SPI_RX_Buff_Ptr_3[0];
  data = SPI_RX_Buff_Ptr_3[1];
  LSB = SPI_RX_Buff_Ptr_3[2];
  bit24_Z = MSB;
  bit24_Z = (bit24_Z << 8) | data;
  bit24_Z = (bit24_Z << 8) | LSB;

  //counter ++;
  if (print2Serial == true) 
  {
    counter++;
    sample_counter = 0;
  
          //Serial.println(bit24_N);
/**
    if (bit24_N >= 8388608 && bit24_N <= 16777215)
    {
      Serial.print((16777215 - bit24_N)*-1);
    }
    else
      Serial.print(bit24_N);

   Serial.print(" ");
     
     
    if (bit24_E >= 8388608 && bit24_E <= 16777215)
    {
      Serial.print((16777215 - bit24_E)*-1);
    }
    else
      Serial.print(bit24_E);

    Serial.print(" ");


    if (bit24_Z >= 8388608 && bit24_Z <= 16777215)
    {
      Serial.println((16777215 - bit24_Z)*-1);
    }
    else
      Serial.println(bit24_Z);*/
   
  Serial.println((String)bit24_N+'*'+(String)bit24_E+'*'+(String)bit24_Z);
  // Serial.println(bit24_Z);
  }
}

void printVoltage()
{ 
  Vout_N = ((float)(bit24_N * VFSR1) / (float)FSR);
  Vout_E = ((float)(bit24_E * VFSR1) / (float)FSR);
  Vout_Z = ((float)(bit24_Z * VFSR1) / (float)FSR);
  
  Serial.print(Vout_N, 5);
  Serial.print(" ");
  Serial.print(Vout_E, 5);
  Serial.print(" ");
  Serial.println(Vout_Z, 5);
  
}

void pinsInput(void)
{
  pinMode(50, INPUT);
  pinMode(51, INPUT);
  pinMode(52, INPUT);
}

// count down timer
void setupTimer()
{
  cli();
  //set timer1 interrupt at 1Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12 and CS10 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  sei();

}
// ISR for counter down timer
ISR(TIMER1_COMPA_vect)
{
  Serial.println(counter);
  //Serial.println("*");
  counter = 0;
}

// ISR for digital pin interrupt
void printCounter()
{
  Serial.println(counter);
  counter = 0;
}

void sendToPython(float *data)
{
  byte *byteData = (byte*)data;
  Serial.write(byteData, 4);
}


