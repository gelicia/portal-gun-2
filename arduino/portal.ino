#include "I2Cdev.h"
#include <OneWire.h>
#include <cobs.h>
#include "MPU6050_6Axis_MotionApps20.h"

//#define TESTING

//pins!
//13 internal LED
//2 dmp interrupt
//4 and 7 button 1 and 2
//5 and 6 pwm 1 and 2
//A2 battery level
//A0 temp
//3 is PWM3

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#endif

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for SparkFun breakout and InvenSense evaluation board)
// AD0 high = 0x69
MPU6050 mpu;

#define LED_PIN 13 // (Arduino is 13, Teensy is 11, Teensy++ is 6)
uint8_t blinkState = 0;

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

uint8_t gloveid; 
uint8_t packet_counter = 0;
uint8_t pin4_1_bucket = 0;
uint8_t pin4_0_bucket = 0;
uint8_t pin7_1_bucket = 0;
uint8_t pin7_0_bucket = 0;

//serial com data
#define INCOMING_BUFFER_SIZE 128
uint8_t incoming_raw_buffer[INCOMING_BUFFER_SIZE];
uint8_t incoming_index = 0;
uint8_t incoming_decoded_buffer[INCOMING_BUFFER_SIZE];

uint8_t crc_error = 0;
uint8_t framing_error = 0;
uint16_t temperature = 0;
unsigned long fps_time = 0; //keeps track of when the last cycle was
uint8_t packets_in_counter = 0;  //counts up
uint8_t packets_in_per_second = 0; //saves the value
uint8_t packets_out_counter = 0;  //counts up
uint8_t packets_out_per_second = 0; //saves the value
uint16_t battery_level = 0;

uint32_t idle_microseconds = 0;
uint8_t cpu_usage = 0;
// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
	mpuInterrupt = true;
}


// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

void setup() {

	pinMode(LED_PIN, OUTPUT);  //indiator LED, onboard yellow
	pinMode(3, OUTPUT);
	pinMode(4, INPUT_PULLUP);
	pinMode(7, INPUT_PULLUP);
	pinMode(5, OUTPUT);
	pinMode(6, OUTPUT);
	pinMode(A0, INPUT);
	pinMode(A2, INPUT);

	reset_output();
	//Serial.println(F("Joining I2C Bus..."));
	// join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
	Wire.begin();
	TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
	Fastwire::setup(400, true);
#endif

	//Serial.println(F("Initializing Serial Port..."));
	Serial.begin(115200);

	//Serial.println(F("Initializing MPU6050 devices..."));
	mpu.initialize();

	// verify connection
	//Serial.println(F("Testing MPU6050 connection..."));
	//Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

	// wait for ready
	//Serial.println(F("\nSend any character to begin DMP programming and demo: "));
	// while (Serial.available() && Serial.read()); // empty buffer
	// while (!Serial.available());                 // wait for data
	// while (Serial.available() && Serial.read()); // empty buffer again

	// load and configure the DMP
	//Serial.println(F("Initializing DMP..."));
	devStatus = mpu.dmpInitialize();


	pinMode(10, INPUT_PULLUP);  //glovemode
	if (digitalRead(10)){
		gloveid = 0;
		// supply your own gyro offsets here, scaled for min sensitivity
		mpu.setXGyroOffset(32);
		mpu.setYGyroOffset(8);
		mpu.setZGyroOffset(5);
		mpu.setXAccelOffset(-1103); // 1688 factory default for my test chip
		mpu.setYAccelOffset(-555); // 1688 factory default for my test chip
		mpu.setZAccelOffset(1296); // 1688 factory default for my test chip
	}
	else{
		gloveid = 1;
		// supply your own gyro offsets here, scaled for min sensitivity
		mpu.setXGyroOffset(26);
		mpu.setYGyroOffset(-61);
		mpu.setZGyroOffset(-1);
		mpu.setXAccelOffset(-2967); // 1688 factory default for my test chip
		mpu.setYAccelOffset(-1571); // 1688 factory default for my test chip
		mpu.setZAccelOffset(1820); // 1688 factory default for my test chip
	}


	// make sure it worked (returns 0 if so)
	if (devStatus == 0) {
		// turn on the DMP, now that it's ready
		//Serial.println(F("Enabling DMP..."));
		mpu.setDMPEnabled(true);

		//Not using Interrupts for IMU, let the FIFO buffer handle it and poll
		//Color sensor interrupts are more important.
		// enable Arduino interrupt detection
		//Serial.println(F("Enabling interrupt detection (Arduino external interrupt 0)..."));
		attachInterrupt(0, dmpDataReady, RISING);

		mpuIntStatus = mpu.getIntStatus();

		// set our DMP Ready flag so the main loop() function knows it's okay to use it
		//Serial.println(F("DMP ready! Waiting for first interrupt..."));
		dmpReady = true;

		// get expected DMP packet size for later comparison
		packetSize = mpu.dmpGetFIFOPacketSize();
	}
	else {
		// ERROR!
		// 1 = initial memory load failed
		// 2 = DMP configuration updates failed
		// (if it's going to break, usually the code will be 1)
		//Serial.print(F("DMP Initialization failed (code "));
		//Serial.print(devStatus);
		//Serial.println(F(")"));
	}

	// configure LED for output


	//GetTempPrep();
}



// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

void loop() {
	// if programming failed, don't try to do anything
	if (!dmpReady) return;


	long int idle_start_timer = 0;

	// wait for MPU interrupt or extra packet(s) available
	while (!mpuInterrupt && fifoCount < packetSize) {

		// other program behavior stuff here
		if (micros() - fps_time > 1000000){
			cpu_usage = 100 - (idle_microseconds / 10000);
			packets_in_per_second = packets_in_counter;
			packets_out_per_second = packets_out_counter;
			if (packets_in_counter == 0) reset_output();
			idle_microseconds = 0;
			packets_in_counter = 0;
			packets_out_counter = 0;
			fps_time = micros();
		}

		//because we are running at 100hz, these buckets can't overflow
		//if running slower use bigger buckets or saturating add
		if (digitalRead(4)) pin4_1_bucket++;
		else				pin4_0_bucket++;
		
		if (digitalRead(7)) pin7_1_bucket++;
		else				pin7_0_bucket++;
		
		temperature = (temperature >> 1) +  (analogRead(A0) >> 1);
		battery_level = (battery_level >> 1) + (analogRead(A2) >> 1);

		SerialUpdate();

		//dont count first cycle, its not idle time, its required
		if (idle_start_timer == 0){
			idle_start_timer = micros();
		}
	}

	idle_microseconds = idle_microseconds + (micros() - idle_start_timer);

	// reset interrupt flag and get INT_STATUS byte
	mpuInterrupt = false;
	mpuIntStatus = mpu.getIntStatus();

	// get current FIFO count
	fifoCount = mpu.getFIFOCount();

	// check for overflow (this should never happen unless our code is too inefficient)
	if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
		// reset so we can continue cleanly
		mpu.resetFIFO();
		//Serial.println(F("FIFO overflow!"));

		// otherwise, check for DMP data ready interrupt (this should happen frequently)
	}
	else if (mpuIntStatus & 0x02) {
		// wait for correct available data length, should be a VERY short wait
		while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

		// read a packet from FIFO
		mpu.getFIFOBytes(fifoBuffer, packetSize);

		// track FIFO count here in case there is > 1 packet available
		// (this lets us immediately read more without waiting for an interrupt)
		fifoCount -= packetSize;


		mpu.dmpGetQuaternion(&q, fifoBuffer);
		mpu.dmpGetGravity(&gravity, &q);
		mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
		mpu.dmpGetAccel(&aa, fifoBuffer);
		mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
		mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);

#ifdef TESTING
		Serial.print("ypr\t");
		Serial.print(ypr[0] * 180 / M_PI);
		Serial.print("\t");
		Serial.print(ypr[1] * 180 / M_PI);
		Serial.print("\t");
		Serial.println(ypr[2] * 180 / M_PI);


		Serial.print("areal\t");
		Serial.print(aaReal.x);
		Serial.print("\t");
		Serial.print(aaReal.y);
		Serial.print("\t");
		Serial.println(aaReal.z);


		Serial.print("aworld\t");
		Serial.print(aaWorld.x);
		Serial.print("\t");
		Serial.print(aaWorld.y);
		Serial.print("\t");
		Serial.println(aaWorld.z);


		Serial.print("Temp: ");
		Serial.println(GetTemp(), 1);
		Serial.println(sizeof(GetTemp()));

#endif

		uint8_t inputs = gloveid << 7;
		if (pin4_1_bucket > pin4_0_bucket) bitSet(inputs, 0);
		if (pin7_1_bucket > pin7_0_bucket) 	bitSet(inputs, 1);

		pin4_1_bucket = 0;
		pin4_0_bucket = 0;
		pin7_1_bucket = 0;
		pin7_0_bucket = 0;

		byte raw_buffer[22];

		raw_buffer[0] = ((((int16_t)(ypr[0] * 18000 / M_PI)) >> 8) & 0xff);
		raw_buffer[1] = ((((int16_t)(ypr[0] * 18000 / M_PI)) >> 0) & 0xff);
		raw_buffer[2] = ((((int16_t)(ypr[1] * 18000 / M_PI)) >> 8) & 0xff);
		raw_buffer[3] = ((((int16_t)(ypr[1] * 18000 / M_PI)) >> 0) & 0xff);
		raw_buffer[4] = ((((int16_t)(ypr[2] * 18000 / M_PI)) >> 8) & 0xff);
		raw_buffer[5] = ((((int16_t)(ypr[2] * 18000 / M_PI)) >> 0) & 0xff);

		raw_buffer[6] = ((aaReal.x >> 8) & 0xff);
		raw_buffer[7] = ((aaReal.x >> 0) & 0xff);
		raw_buffer[8] = ((aaReal.y >> 8) & 0xff);
		raw_buffer[9] = ((aaReal.y >> 0) & 0xff);
		raw_buffer[10] = ((aaReal.z >> 8) & 0xff);
		raw_buffer[11] = ((aaReal.z >> 0) & 0xff);

		raw_buffer[12] = ((aaWorld.x >> 8) & 0xff);
		raw_buffer[13] = ((aaWorld.x >> 0) & 0xff);
		raw_buffer[14] = ((aaWorld.y >> 8) & 0xff);
		raw_buffer[15] = ((aaWorld.y >> 0) & 0xff);
		raw_buffer[16] = ((aaWorld.z >> 8) & 0xff);
		raw_buffer[17] = ((aaWorld.z >> 0) & 0xff);
	
		raw_buffer[18] = inputs;

		raw_buffer[19] = cpu_usage;

		raw_buffer[20] = ((temperature >> 8) & 0xff); 
		raw_buffer[21] = ((temperature >> 0) & 0xff);


		raw_buffer[22] = packets_in_per_second;
		raw_buffer[23] = packets_out_per_second;

		raw_buffer[24] = framing_error;
		raw_buffer[25] = crc_error;

		raw_buffer[26] = ((battery_level >> 8) & 0xff);
		raw_buffer[27] = ((battery_level >> 0) & 0xff);

		raw_buffer[28] = packet_counter++;
		raw_buffer[29] = OneWire::crc8(raw_buffer, 29);

		//prep buffer completely
		uint8_t encoded_buffer[31];  //one extra to hold cobs data
		uint8_t encoded_size = COBSencode(raw_buffer, 30, encoded_buffer);
		//send out data from last cycle
		Serial.write(encoded_buffer, encoded_size);
		Serial.write(0x00);

		//will overflow if not reset once a second....
		packets_out_counter++;
		
		SerialUpdate();
	}
}

void onPacket(const uint8_t* buffer, size_t size)
{

	//format of packet is
	//PWM1 PWM2 PWM3 CRC

	//check for framing errors
	if (size != 4 ){
		framing_error++;
	}
	else{
		//check for crc errors
		byte crc = OneWire::crc8(buffer, size - 1);
		if (crc != buffer[size - 1]){
			crc_error++;
		}
		else{	
				//increment packet stats counter
				if (packets_in_counter < 255){
					packets_in_counter++;
				}

				// blink LED to indicate activity
				blinkState++;
				digitalWrite(LED_PIN, bitRead(blinkState, 2));

				//output pwm data
				analogWrite(5, buffer[0]);
				analogWrite(6, buffer[1]);
				analogWrite(3, buffer[2]);
		}
	}
}

void SerialUpdate(void){
	while (Serial.available()){

		//read in a byte
		incoming_raw_buffer[incoming_index] = Serial.read();

		//check for end of packet
		if (incoming_raw_buffer[incoming_index] == 0x00){

			//try to decode
			uint8_t decoded_length = COBSdecode(incoming_raw_buffer, incoming_index, incoming_decoded_buffer);

			//check length of decoded data (cleans up a series of 0x00 bytes)
			if (decoded_length > 0)	onPacket(incoming_decoded_buffer, decoded_length);

			//reset index
			incoming_index = 0;
		}
		else{
			//read data in until we hit overflow, then hold at last position
			incoming_index++;
			if (incoming_index == INCOMING_BUFFER_SIZE) incoming_index = 0;
				
		}
	}
}

void reset_output(void) {
	blinkState = 0;
	digitalWrite(LED_PIN, bitRead(blinkState, 2));

	//output pwm data
	analogWrite(5, 0);
	analogWrite(6, 0);
	analogWrite(3, 0);
}