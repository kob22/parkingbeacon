#include <NewPing.h>
#include <QueueList.h>
#include <SPI.h>
#include <RF24.h>
#include <BTLE.h>

RF24 radio(53,48);
BTLE btle(&radio);

#define SONAR_NUM     3 // liczba czujnikow
#define MAX_DISTANCE 50 // maksymalny dystans
#define PING_INTERVAL 300 // milisekundy co ile pinguje

unsigned long pingTimer[SONAR_NUM]; // czas kiedy czujnik powinien pingowac
unsigned int cm[SONAR_NUM];         // odleglosci odczytanie przez czujnik
uint8_t currentSensor = 0;          // aktualny czujnik

NewPing sonar[SONAR_NUM] = {     // tablica czujnikow
  NewPing(3, 2, MAX_DISTANCE), // trigger pin, echo pin, maksymalny dystans
  NewPing(5, 4, MAX_DISTANCE),
  NewPing(7, 6, MAX_DISTANCE) 
};

unsigned int diody[3][2]={{34,32},{30,28},{26,24}}; // wyjscia na diody
QueueList <int> ping_queue[SONAR_NUM] = {QueueList<int>(), QueueList<int>(), QueueList<int>()} ; // kolejki z odczytami
unsigned int counter[SONAR_NUM]={10,10,10}; // liczba pomiarow
unsigned int before[SONAR_NUM] = {1,1,1}; // tablica pomocnicza, zapisany stan miejsca przed odczytem 
unsigned int after[SONAR_NUM] ={0,0,0}; // tablica pomocnicza, zapisany stan miejsca po odczycie
unsigned int countFree=0;

void setup() {
  Serial.begin(9600);
  
  btle.begin("Parking"); // start bluetooth
  for (uint8_t i = 0; i<3;i++)
  {
    pinMode(diody[i][0], OUTPUT);
    pinMode(diody[i][1], OUTPUT);    
  }
   for (uint8_t i = 0; i<10;i++)
  {
    ping_queue[0].push(0);
    ping_queue[1].push(0);
    ping_queue[2].push(0);
  }
  pingTimer[0] = millis() + 75;           
  for (uint8_t i = 1; i < SONAR_NUM; i++) 
    pingTimer[i] = pingTimer[i - 1] + PING_INTERVAL;

}

void loop() {

  //wysylanie liczby miejsc
  nrf_service_data buf;
  buf.service_uuid = 0x2A29;
  buf.value = countFree;
  
  if(!btle.advertise(0x16, &buf, sizeof(buf))) {
    Serial.println("BTLE advertisement failure");
  }
  btle.hopChannel();
  
  for (uint8_t i = 0; i < SONAR_NUM; i++) { // Loop through all the sensors.
    if (millis() >= pingTimer[i]) {         // Is it this sensor's time to ping?
      pingTimer[i] += PING_INTERVAL * SONAR_NUM;  // Set next time this sensor will be pinged.
      if (i == 0 && currentSensor == SONAR_NUM - 1) oneSensorCycle(); // Sensor ping cycle complete, do something with the results.
      sonar[currentSensor].timer_stop();          // Make sure previous timer is canceled before starting a new ping (insurance).
      currentSensor = i;                          // Sensor being accessed.
      cm[currentSensor] = 0;                      // Make distance zero in case there's no ping echo for this sensor.
      sonar[currentSensor].ping_timer(echoCheck); // Do the ping (processing continues, interrupt will call echoCheck to look for echo).
    }
  }
  // Other code that *DOESN'T* analyze ping results can go here.
}

void echoCheck() { // If ping received, set the sensor distance to array.
  if (sonar[currentSensor].check_timer())
    cm[currentSensor] = sonar[currentSensor].ping_result / US_ROUNDTRIP_CM;
}

void oneSensorCycle() { // Sensor ping cycle complete, do something with the results.
  countFree = 0;
  for (uint8_t i = 0; i < SONAR_NUM; i++) {
    Serial.print(i);
    Serial.print("=");
    Serial.print(cm[i]);
    Serial.print("cm ");

    // dodawanie i usuwanie do kolejki odczytow
    if (ping_queue[i].pop() == 0)
    {
      counter[i]--;
    }
    ping_queue[i].push(cm[i]);
    if (cm[i] ==0)
    {
      counter[i]++;
    }
    if (counter[i]>5){
      after[i] = 0;  
    }
    else
    {
      after[i] = 1;
    }
    //ustawianie koloru diody zajete/wolne miejsce
    if (after[i] == 0 && before[i] == 1){
    digitalWrite(diody[i][1], LOW);
    digitalWrite(diody[i][0], HIGH);
 
    }
    if (after[i]==1 && before[i] == 0)
    {
      digitalWrite(diody[i][0], LOW);
      digitalWrite(diody[i][1], HIGH);

    }
    before[i] = after[i];
    if(after[i] == 0)
      countFree++;
  }


  Serial.println();
}
