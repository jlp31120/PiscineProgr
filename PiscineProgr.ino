/////////////////////////////////////////////////////////////////////////////////////
//
// JLP mars 2021  programmateur piscine
//                à partir de programmateur "volet avec calcul éphéméride" 
//                arduino NANO
//                module tiny RTC 
//                module with LCD keypad
//                
//
//  automate d'affichage date heure  + deux zones date heure
//           et  écriture dans la ROM 
// 
// 29/03/2021   mise en place source
// 25/09/2021  intégration boite tests
//              quelques modifs dont le mode >-3
//              vérifier la tpr min raz et comportemment rajouter un flags ça gèle
// en marche une heure et RAZ sur heure suivante avec test tpr
//   version debug sans SDREADER
//   Program size: 16 970 bytes (used 55% of a 30 720 byte maximum) (3,58 secs)
//   Minimum Memory Usage : 1197 bytes(58 % of a 2048 byte maximum)
//   prog trop gros pour NANO !
//
// 07/10/2021 mise en mémoire programme des const 
//         utilisation de PROGMEM et des fonctions de lectures qui vont avec
//         Début des test, il faut changer l'alim qui n'est plus suffisente avec SD card
//         version release
//         Program size: 22 486 bytes (used 73% of a 30 720 byte maximum) (2,35 secs)
//         Minimum Memory Usage : 1243 bytes(61 % of a 2048 byte maximum)
//       
// 17/11/2021 après plantage ordi
//        changement écran pour LCDIIC
// 
/////////////////////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------------------
//#undef DEBUG
#define DEBUG
//#define WD00  // watchdog 
//#undef WD00  // watchdog 
//#define SDREADER
#undef SDREADER
#define myARDUINO_AVR_NANO
//#undef myARDUINO_AVR_NANO
//---------------------------------------------------------------------------------
#include <avr/io.h>   
#include <avr/interrupt.h>
#include <avr/wdt.h>
//#include <util/delay.h>
#include <avr/pgmspace.h>

//#include <TimeLib.h> fait par DS1307RTC.h
#include <OneWire.h> 
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t modifiée pour avoir la gestion de la ram
#include <LiquidCrystal.h>

//#include <elapsedMillis.h>
//#include <RTClib.h>   ne pas utiliser
//#include <dht.h>

// pour sd reader
#ifdef SDREADER
#include <SPI.h>
#include <SD.h>
File myFile;
#endif // SDREADER

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

#define WAIT_1S 1000L
#define WAIT_9S 9000L

#define  A_SHOW_DATE  10

#define  Z_SET_JJ  30
#define  Z_SET_MM  31
#define  Z_SET_AA  32
#define  Z_SET_HH  33
#define  Z_SET_MN  34
#define  Z_SET_PG  35
#define  Z_SET_AM  36

#define  posHH 7
#define  posDelta  15

// Code de retour de la fonction getTemperature() supprimé pour de la place
/*enum DS18B20_RCODES {
    READ_OK,  // Lecture ok
    NO_SENSOR_FOUND,  // Pas de capteur
    INVALID_ADDRESS,  // Adresse reçue invalide
    INVALID_SENSOR  // Capteur invalide (pas un DS18B20)
};
*/

//---------------------------------------------------------------------------------
typedef struct _sAction {
    char t0;
    char t1;
    int8_t nProg;
    char Auto;
    char f;
} sAction;

const int8_t cSizeSAction = sizeof(_sAction);

union _uAction {
  _sAction  s;
  byte gRam[cSizeSAction];
} uAction;
_uAction  gT_Action; 
//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------
const char p01[] PROGMEM = "Pg <-3\xDF";
const char p02[] PROGMEM = "Pg<= 0\xDF";
const char p03[] PROGMEM = "Pg <20\xDF";
const char p04[] PROGMEM = "Pg <28\xDF";
const char p05[] PROGMEM = "Pg 28\xDF ";
const char p06[] PROGMEM = "Pg <31\xDF";
const char p07[] PROGMEM = "Pg 31\xDF ";
const char p08[] PROGMEM = "Pg 32\xDF ";
const char p09[] PROGMEM = "Pg 33\xDF ";
const char p10[] PROGMEM = "Pg 34\xDF ";
const char p11[] PROGMEM = "Pg >34\xDF";

struct sR {
    const char* NomPg;
    const uint8_t HH[24];
};

/*
en fonction de la temperature et de l'heure,
 ->  Hexa 4 bits qui correspondent  à chaque quart d'heure d'une heure
     1 +2 +4 +8  l'heure entière
     4 + 8        la dernière demi heure
*/
const sR tRemote[] PROGMEM = {
 // HEURES ", {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 },
    {   p01 , {  3, 0, 1, 3, 0,15, 5, 1, 1, 0, 1, 0, 0, 0, 3, 0, 1, 0, 1, 0, 1, 0,15, 1 } }, // 1
    {   p02 , {  1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0 } },  // 2
    {   p03 , {  0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2 } }, // 3
    {   p04 , {  0, 0, 0, 0, 0, 0, 0,15,15, 0, 0, 0,15,15, 1, 0, 0, 0,15,15,15,15, 0, 0 } }, // 4
    {   p05 , {  0, 0, 0, 0, 0, 0, 0,15,15, 0, 0, 0,15,15, 1, 0, 0, 0,15,15,15,15, 0, 0 } }, // 5
    {   p06 , {  0, 0, 0, 0, 0, 0, 8,15,15, 0, 0, 0,15,15, 3, 0, 0, 0,15,15,15,15, 0, 0 } }, // 6
    {   p07 , {  0, 0, 0, 0, 0, 0,12,15,15, 0, 0, 0,15,15, 7, 0, 0, 0,15,15,15,15, 0, 0 } }, // 7
    {   p08 , {  0, 0, 0, 0, 0, 0,14,15,15, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15,15, 0, 1 } }, // 8
    {   p09 , {  0, 0, 0, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15,15, 0, 3 } }, // 9
    {   p10 , {  0, 0, 0, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15,15, 0, 7 } }, // 10
    {   p11 , { 15, 0, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15,15, 0,15, 0 } }  // 11
} ;

const int8_t MAXPROG = sizeof(tRemote) / sizeof(tRemote[0])-1;


TimeElements gTm, gTmRef;
int lcd_key = 0;

bool  FlagMinute    = false;
bool  FlagHeure     = false;
bool  FlagJour      = false;
bool  FlagSeconde   = false;


int   iWRAZ         = -1;
int   Compteur10s   = 0;
/*
int   Compteur60s   = 0;
int   Compteur60mn = 0;
*/
int iRobot = A_SHOW_DATE;
unsigned long previousMillis = 0;

String  Message;

int   DayRef, Href, MnRef;
int  TprNow = -99;

int  TprJMax = -99 , TprJMin = 99;

byte data[9], addr[8];
byte bVal = 0;

#define PIN_KBD      PIN_A0
#define PIN_ONEWIRE  PIN_A1
#define PinCMD_MOTOR PIN_A2


//---------------------------------------------------------------------------------
#ifdef myARDUINO_AVR_NANO 
LiquidCrystal lcd(PD6, PD7, PD2, PD3, PD4, PD5); //arduino NANO
#else
 //LiquidCrystal lcd(11, 12, 2, 3, 4, 5); // shield arduino UNO 
 LiquidCrystal lcd(8, 9, 4, 5, 6, 7); 
#endif // myARDUINO_AVR_NANO

//  Création  OneWire pourtemperature
OneWire ds(PIN_ONEWIRE);
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

#ifdef SDREADER
void fwriteSD(const char * lg )
{
// On the Ethernet Shield, CS is pin 4.  It's set as an output by default.
// Note that even if it's not used as the CS pin, the hardware SS pin 
//  (10 on most Arduino boards, 53 on the Mega)  must be left as an output 
//  or the SD library functions will not work.
    pinMode(10, OUTPUT);
    if (!SD.begin( 10)) {
        #ifdef DEBUG
        Serial.println(F(" SD Err"));
        #endif // DEBUG
        return;
    }
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    myFile = SD.open("J.txt", FILE_WRITE);
    if (myFile) {
        myFile.print(lg);
        myFile.close();
        #ifdef DEBUGs
        Serial.print(F("\n\t\tSD = "));
        Serial.println(lg);
        #endif // DEBUG

    }
    else {
        #ifdef DEBUG
        Serial.println(F("SD Err W"));
        #endif // DEBUG
    }
}
#endif // SDREADER
//---------------------------------------------------------------------------------
//permet d'afficher le char °
//---------------------------------------------------------------------------------
void AffNomPgnToLCD(int n) {
  lcd.print((const __FlashStringHelper*)pgm_read_word(&(tRemote[n].NomPg)));
}

//---------------------------------------------------------------------------------
//permet d'afficher les nombres sur deux chiffres
//---------------------------------------------------------------------------------
char *Vers2Chiffres(char* resultat, const uint8_t valeur) {
uint8_t Q, R;
static const char num[] PROGMEM = { '0','1','2','3','4','5','6','7','8','9' };
    R = valeur % 10;
    Q = valeur / 10;
    if (Q == 0) {
        resultat[0] = '0';
        resultat[1] = pgm_read_byte(&num[R]);
 }
    else {
        resultat[0] = pgm_read_byte(&num[Q]);
        resultat[1] = pgm_read_byte(&num[R]);
    }
    resultat[2] = '\0';
    return (resultat);
}

//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------
/*
String Vers2Chiffres(byte nombre) {
    String resultat = "";
    if (nombre < 10)
        resultat = "0";
    return resultat += String(nombre, DEC);
}
*/
//---------------------------------------------------------------------------
// affiche la date et l'heure sur l'écran
//---------------------------------------------------------------------------
void show_DtH(boolean bCmd) {
    int n;
    char buf[18], tp[3];
     n = gT_Action.s.nProg;
     //------------------------------------------------------------
     // preparation ligne 0
     //------------------------------------------------------------
     // Date  Heure
     Vers2Chiffres(tp, gTm.Day);
     buf[0] = tp[0];
     buf[1] = tp[1];
     *(buf + 2) = '/';
     Vers2Chiffres(tp, gTm.Month);
     buf[3] = tp[0];
     buf[4] = tp[1];
     *(buf + 5) = '/';
     *(buf + 6) = '2';
     *(buf + 7) = '0';
     Vers2Chiffres( tp, (gTm.Year+1970)%2000 );
     buf[8] = tp[0];
     buf[9] = tp[1];
     *(buf + 10) = ' ';
     Vers2Chiffres(tp, gTm.Hour);
     buf[11] = tp[0];
     buf[12] = tp[1];
     *(buf + 13) = ':';
      Vers2Chiffres(tp,gTm.Minute);
      buf[14] = tp[0];
      buf[15] = tp[1];
      buf[16] = '\0';

      lcd.clear();
      lcd.noBlink();
      lcd.noCursor();
      //------------------------------------------------------------
      // ligne 0
      //------------------------------------------------------------
      lcd.setCursor(0, 0);

      lcd.print( buf );
      buf[16] = ' ';
      buf[17] = '\0';
      #ifdef DEBUG
      Serial.print( buf );
      #endif
      #ifdef SDREADER
      fwriteSD(buf);
      #endif

     //------------------------------------------------------------
     // preparation ligne 1
     //------------------------------------------------------------
     if (TprNow > 0) {
         Vers2Chiffres(tp, TprNow);
         buf[0] = '+';
         buf[1] = tp[0];
         buf[2] = tp[1];
     }
     else if (TprNow < 0) {
         Vers2Chiffres(tp, (-1* TprNow));
         buf[0] = '-';
         buf[1] = tp[0];
         buf[2] = tp[1];

     }
     else {
         buf[0] = ' ';
         buf[1] = ' ';
         buf[2] = '0';
     };
     buf[3] = (char)223;
     buf[4] = ' ';
     buf[5] =  gT_Action.s.Auto;
     buf[6] = ' ';
     buf[7] = bCmd?'0':'1';
     buf[8] = ' ';
     buf[9] = '\0';
 
     //------------------------------------------------------------
     // ligne 1
     //------------------------------------------------------------
     lcd.setCursor(0, 1);
     lcd.print(buf);

     #ifdef DEBUG 
       Serial.print("\t");
       buf[3] = ' ';
       Serial.print(buf);
     #endif
     #ifdef SDREADER
     buf[3] = ' ';
     fwriteSD(buf);
     #endif
     //------------------------------------------------------------

     //------------------------------------------------------------
     lcd.setCursor(9, 1);
     //------------------------------------------------------------
     AffNomPgnToLCD(n); // sprintf pour degre 0x223'
     
// changé mais  à voir si ça marche ???
//  strcpy_PF(buf, (__FlashStringHelper*)pgm_read_ptr(&(tRemote[n].NomPg)));
     strcpy_PF(buf, pgm_read_ptr(&(tRemote[n].NomPg)));
     buf[6] = ' ';
    #ifdef DEBUG
     Serial.println(buf);
    #endif
    #ifdef SDREADER
    // strcat(buf, "\n");
     fwriteSD(buf);
    #endif
         
}
//---------------------------------------------------------------------------
void SauveDansRTC(void)
{
    RTC.writenvram(0, gT_Action.gRam, cSizeSAction);
#ifdef DEBUGx
    char sDebug[80];
    sprintf(sDebug, " taille struct sAction %u", cSizeSAction);
    Serial.println(sDebug);
#endif
}
//---------------------------------------------------------------------------
//  utilisé pour initialiser ave DATE compil
//---------------------------------------------------------------------------
static uint8_t conv2d(const char* p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
        v = *p - '0';
    return 10 * v + *++p - '0';
}
//---------------------------------------------------------------------------
//  convertit un  date time en struct tm
//  utilisé pour initialiser ave DATE compil
//---------------------------------------------------------------------------
TimeElements  StrDtToTm(const char* date, const char* time) {
    TimeElements tm;

    // sample input: date = "Dec 26 2009"
    uint8_t m;
    RTC.read(tm);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    switch (date[0]) {
//    case 'J': m = (date[1] == 'a') ? 1 : (m = (date[2] == 'n') ? 6 : 7);   break;
    case 'J': if (date[1] == 'a')       m = 1;
            else if (date[2] == 'n')    m = 6;
            else                        m = 7;
        break;
    case 'F': m = 2; break;
    case 'A': m = date[2] == 'r' ? 4 : 8; break;
    case 'M': m = date[2] == 'r' ? 3 : 5; break;
    case 'S': m = 9; break;
    case 'O': m = 10; break;
    case 'N': m = 11; break;
    case 'D': m = 12; break;
    }
    tm.Day = conv2d(date + 4);
    tm.Month = m;
    tm.Year = conv2d(date + 9) + 30;
    tm.Hour = conv2d(time + 0);
    tm.Minute = conv2d(time + 3);
    tm.Second = conv2d(time + 6);
    return (tm);
}
///////////////////////////////////////////////////////////////////////////////
byte fSetupCapteurTpr(void) {
  
//     Reset le bus 1-Wire si nécessaire (requis pour la lecture du premier capteur) 
    ds.reset_search();

//     Recherche le prochain capteur 1-Wire disponible 
    if (!ds.search(addr)) {
        // Pas de capteur
        return  1;
    }
    // Vérifie que l'adresse a été correctement reçue 
    if (OneWire::crc8(addr, 7) != addr[7]) {
        return 2;
    }
    // Vérifie qu'il s'agit bien d'un DS18B20 
    if (addr[0] != 0x28) {
        // Mauvais type de capteur
        return 3;
    }
    return 0;
}
///////////////////////////////////////////////////////////////////////////////
// 
// Fonction de lecture de la température via un capteur DS18B20.
// 
///////////////////////////////////////////////////////////////////////////////
void getTemperature(float* temperature) {
    // Reset le bus 1-Wire et sélectionne le capteur 
   ds.reset();
#ifdef DEBUGx
    char sDebug[80];
    sprintf(sDebug, " ds.reset()= %u", res );
    Serial.println(sDebug);
#endif
    ds.select(addr);

    // Lance une prise de mesure de température et attend la fin de la mesure 
    ds.write(0x44, 1);
    delay(800);

     // Reset le bus 1-Wire, sélectionne le capteur et envoie une demande de lecture du scratchpad 
    ds.reset();
    ds.select(addr);
    ds.write(0xBE);

    // Lecture du scratchpad 
    for (byte i = 0; i < 9; i++) data[i] = ds.read();
    // Calcul de la température en degré Celsius 
    *temperature = ((data[1] << 8) | data[0]) * 0.0625;
}
//---------------------------------------------------------------------------
//  toutes les minutes
//  renvoie la temperature mesurée ou -99 après 5 essais
//---------------------------------------------------------------------------
int  fRMesure(void)
{
    int tpr;
    float temperature;
    for (int i = 0; i <= 5; i++) {
        getTemperature(&temperature);
        tpr = int(temperature);
        if ( (tpr > -50) && (tpr < 50)) {
            break;
        }
        else {
            tpr = -99;
            delay(50);
            #ifdef DEBUG
            Serial.print(F("."));
            #endif // DEBUG
        }
    }
    return tpr;
};
//---------------------------------------------------------------------------
int read_LCD_buttons() {               // read the buttons
int uKeyOld = 0,
    uKey = 0;

#ifdef WD00
    wdt_reset(); // jlp watchdog 2016 09 24
#endif  // WD00
uKey = analogRead(PIN_KBD);       // read the value from the sensor 

if ( uKey > 900 ) return btnNONE; // aucune touche appuyée 

    do {
        uKeyOld = uKey;
        delay(10);
        uKey = analogRead(PIN_KBD);
    } while ( uKey != uKeyOld );

#ifdef DEBUGb
    Serial.println("LCD_btn");
#endif // DEBUG


    do {	// attendre que la touche soit relachée	
    } while ( analogRead(PIN_KBD) < 900 );

    //	uKey = uKeyOld;
    /*
       // For V1.1 us this threshold
       if (uKey < 50)   return btnRIGHT;
       if (uKey < 250)  return btnUP;
       if (uKey < 450)  return btnDOWN;
       if (uKey < 650)  return btnLEFT;
       if (uKey < 850)  return btnSELECT;
   */
   // For V1.0 comment the other threshold and use the one below:

    Compteur10s = 0; // clear the timer 
#ifdef DEBUGxx
    if (uKey < 70)        Serial.println("btnLEFT   " + String(uKey, DEC));
    else if (uKey < 195)  Serial.println("btnUP     " + String(uKey, DEC));
    else if (uKey < 380)  Serial.println("btnDOWN   " + String(uKey, DEC));
    else if (uKey < 555)  Serial.println("btnRIGHT   " + String(uKey, DEC));
    else if (uKey < 790)  Serial.println("btnSELECT " + String(uKey, DEC));

#endif // DEBUG

    if (uKey < 70)   return btnLEFT;
    if (uKey < 195)  return btnUP;
    if (uKey < 380)  return btnDOWN;
    if (uKey < 555)  return btnRIGHT;
    if (uKey < 790)  return btnSELECT;


    return btnNONE;                // when all others fail, return this.
}
//---------------------------------------------------------------------------
//  montre date heure
//---------------------------------------------------------------------------
int fShowDate(int iOldRobot)
{

    lcd_key = read_LCD_buttons();   // readn the buttons
    switch (lcd_key) {
    case btnSELECT: 
        return (Z_SET_PG);
        break;
    
    default :
        return (iOldRobot);
    }

}
//---------------------------------------------------------------------------
//  montre  heure ON et Heure OF
//---------------------------------------------------------------------------
int fShowONOF(int iOldRobot)
{
    lcd_key = read_LCD_buttons();   // read the buttons
    switch (lcd_key) {
    case btnUP:
    case btnDOWN:
        return (A_SHOW_DATE);
        break;

    default:
        return (iOldRobot);
    }
}
//---------------------------------------------------------------------------
//  init Horloge Jour
//---------------------------------------------------------------------------
int fSet_JJ(int iOldRobot)
{
    char tp[3];
    lcd.setCursor(1, 0);
    lcd.blink();
    lcd_key = read_LCD_buttons();   // read the buttons
    switch (lcd_key) {
    case btnRIGHT: {
        return (Z_SET_MM);
        break;
    }
    case btnLEFT: {
        return (Z_SET_PG);
        break;
    }

    case btnUP: {
        gTm.Day += 1;
        if (gTm.Day > 31 ) gTm.Day = 1;
        lcd.setCursor(0, 0);
        lcd.print(Vers2Chiffres(tp, gTm.Day));
        lcd.setCursor(1, 0);
        break;
    }

    case btnDOWN: {
        gTm.Day -= 1;
        if (gTm.Day < 1 ) gTm.Day = 28;
        lcd.setCursor(0, 0);
        lcd.print(Vers2Chiffres(tp, gTm.Day));
        lcd.setCursor(1, 0);
        break;
    }
    case btnSELECT: {
        gTm.Second = 0;
        gTmRef = gTm;
        RTC.write(gTm);
        return (A_SHOW_DATE);
        break;
    }

    }
    return(iOldRobot);
}
//---------------------------------------------------------------------------
//  init Horloge MOIS
//---------------------------------------------------------------------------
int fSet_MM(int iOldRobot)
{
    char tp[3];

    lcd.setCursor(4, 0);
    lcd.blink();
    lcd_key = read_LCD_buttons();   // read the buttons
    switch (lcd_key) {
    case btnRIGHT: {
        return (Z_SET_AA);
        break;
    }
    case btnLEFT: {
        return (Z_SET_JJ);
        break;
    }

    case btnUP: {
        gTm.Month += 1;
        if (gTm.Month > 12) gTm.Month = 1;
        lcd.setCursor(3, 0);
        lcd.print(Vers2Chiffres(tp,gTm.Month));
        lcd.setCursor(4, 0);
        break;
    }

    case btnDOWN: {
        if (gTm.Month > 1) gTm.Month -= 1;
        lcd.setCursor(3, 0);
        lcd.print(Vers2Chiffres(tp,gTm.Month));
        lcd.setCursor(4, 0);
        break;
    }
    case btnSELECT: {
        gTm.Second = 0;
        gTmRef = gTm;
        RTC.write(gTm);
        return (A_SHOW_DATE);
        break;
    }

    }
    return(iOldRobot);
}
//---------------------------------------------------------------------------
//  init Horloge AN
//---------------------------------------------------------------------------
int fSet_AA(int iOldRobot)
{
    lcd.setCursor(9, 0);
    lcd.blink();
    lcd_key = read_LCD_buttons();   // read the buttons
    switch (lcd_key) {
    case btnRIGHT: {
        return (Z_SET_HH);
        break;
    }
    case btnLEFT: {
        return (Z_SET_MM);
        break;
    }

    case btnUP: {
        gTm.Year += 1;
        lcd.setCursor(6, 0);
        lcd.print(tmYearToCalendar(gTm.Year));
        lcd.setCursor(9, 0);
        break;
    }

    case btnDOWN: {
         if (gTm.Year > 51 ) gTm.Year -= 1;
        #ifdef DEBUG
         //		Serial.println(" année " + String(tm.Year, DEC) );
        #endif
        lcd.setCursor(6, 0);
        lcd.print(tmYearToCalendar(gTm.Year));
        lcd.setCursor(9, 0);
        break;
    }
    case btnSELECT: {
        gTm.Second = 0;
        gTmRef = gTm;
        RTC.write(gTm);
        return (A_SHOW_DATE);
        break;
    }

    }
    return(iOldRobot);
}
//---------------------------------------------------------------------------
//  init Horloge HEURE
//---------------------------------------------------------------------------
int fSet_HH(int iOldRobot)
{
    char tp[3];
    lcd.setCursor(12, 0);
    lcd.blink();
    lcd_key = read_LCD_buttons();   // read the buttons
    switch (lcd_key) {
    case btnRIGHT: {
        return (Z_SET_MN);
        break;
    }
    case btnLEFT: {
        return (Z_SET_AA);
        break;
    }

    case btnUP: {
        if (gTm.Hour > 22) gTm.Hour = 0;
        else              gTm.Hour += 1;
        lcd.setCursor(11, 0);
        lcd.print(Vers2Chiffres(tp,gTm.Hour));
        lcd.setCursor(12, 0);
        break;
    }

    case btnDOWN: {
        if (gTm.Hour < 1) gTm.Hour = 23;
        else              gTm.Hour -= 1; 
        lcd.setCursor(11, 0);
        lcd.print(Vers2Chiffres(tp,gTm.Hour));
        lcd.setCursor(12, 0);
        break;
    }
    case btnSELECT: {
        gTm.Second = 0;
        gTmRef = gTm;
        RTC.write(gTm);
        return (A_SHOW_DATE);
        break;
    }

    }
    return(iOldRobot);
}
//---------------------------------------------------------------------------
//  init Horloge MINUTE
//---------------------------------------------------------------------------
int fSet_MN(int iOldRobot)
{
    char tp[3];
    lcd.setCursor(15,0);
    lcd.blink();
    lcd_key = read_LCD_buttons();   // read the buttons
    switch (lcd_key) {
    case btnRIGHT: {
        return (Z_SET_AM);
        break;
    }
    case btnLEFT: {
        return (Z_SET_HH);
        break;
    }

    case btnUP: {
        if (gTm.Minute > 58) gTm.Minute = 0;
        else                gTm.Minute += 1;
        lcd.setCursor(14, 0);
        lcd.print(Vers2Chiffres(tp,gTm.Minute));
        lcd.setCursor(15, 0);
        break;
    }

    case btnDOWN: {
        if (gTm.Minute < 1 ) gTm.Minute = 59;
        else                 gTm.Minute -= 1;
        lcd.setCursor(14, 0);
        lcd.print(Vers2Chiffres(tp,gTm.Minute));
        lcd.setCursor(15, 0);
        break;
    }
    case btnSELECT: {
        gTm.Second = 0;
        gTmRef = gTm;
        RTC.write(gTm);
        return (A_SHOW_DATE);
        break;
    }

    }
    return(iOldRobot);
}
//---------------------------------------------------------------------------
//  init programme
//---------------------------------------------------------------------------
int fSet_PG(int iOldRobot)
{
    lcd.setCursor(9, 1);
    lcd.blink();
    lcd_key = read_LCD_buttons();   // read the buttons
    switch (lcd_key) {
    case btnRIGHT: 
        return (Z_SET_JJ);
        break;
    
    case btnLEFT: 
        return (Z_SET_AM);
        break;
    
    case btnUP: 
        gT_Action.s.nProg++;
        if (gT_Action.s.nProg > MAXPROG)  gT_Action.s.nProg = 0;
        AffNomPgnToLCD(gT_Action.s.nProg); // sprintf pour degre 0x223'
        break;
   
    case btnDOWN: 
        gT_Action.s.nProg--;
        if (gT_Action.s.nProg < 0 )  gT_Action.s.nProg = MAXPROG;
        AffNomPgnToLCD(gT_Action.s.nProg); // sprintf pour degre 0x223'
        break;
    
    case btnSELECT: 
        SauveDansRTC();  // adresse 0  sauve nProg
        gTmRef = gTm;
        RTC.write(gTm);
        FlagJour = true;  // init du programme
        return (A_SHOW_DATE);
        break;
    }
    return(iOldRobot);
}
//---------------------------------------------------------------------------
//  programme en AUTO ou en Manuel
//---------------------------------------------------------------------------
int fSet_AM(int iOldRobot)
{
    lcd.setCursor(5, 1);
    lcd.blink();
    lcd_key = read_LCD_buttons();   // read the buttons
    switch (lcd_key) {
    case btnRIGHT: 
        return (Z_SET_PG);
        break;
    
    case btnLEFT: 
        return (Z_SET_MN);
        break;

    case btnUP: 
    case btnDOWN: 
        if (gT_Action.s.Auto == 'M') gT_Action.s.Auto = 'A';
        else                         gT_Action.s.Auto = 'M';
        lcd.print(gT_Action.s.Auto);
        break;
    
    case btnSELECT: 
        SauveDansRTC();  // adresse 0  sauve nProg
        gTmRef = gTm;
        RTC.write(gTm);
        if (gT_Action.s.Auto == 'A')   FlagJour = true;  // init du programme
        return (A_SHOW_DATE);
        break; 
    }
    return(iOldRobot);
}
//---------------------------------------------------------------------------
// tous les jours si automatique
//---------------------------------------------------------------------------
void fQuelProg(void) {
    int tpr;
    tpr = TprJMax;
    /*
    Serial.print("\tTprJMax ");
    Serial.print(TprJMax);
    Serial.print(" ");
    Serial.print("TprJMin");
    Serial.print(TprJMin);
    Serial.print("\n");
    */
    if ( TprJMin < -3 )  gT_Action.s.nProg = 0;  // min plus petit que -3
    else if (tpr <= 0)   gT_Action.s.nProg = 1;  // plus petit que 0
    else if (tpr < 20)   gT_Action.s.nProg = 2;  // 
    else if (tpr < 28)   gT_Action.s.nProg = 3;  // 
    else if (tpr == 28)  gT_Action.s.nProg = 4;  // 
    else if (tpr < 31)   gT_Action.s.nProg = 5;  // 
    else if (tpr ==31)   gT_Action.s.nProg = 6;  // 
    else if (tpr == 32)  gT_Action.s.nProg = 7;  // 
    else if (tpr == 33)  gT_Action.s.nProg = 8;  // 
    else if (tpr == 34)  gT_Action.s.nProg = 9;  // 
    else                 gT_Action.s.nProg = 10;  // 

}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void fActionMoteur(boolean bCmd) {
#ifdef DEBUGs
      if (bON)      Serial.println("  ON ");
      else          Serial.println("  OFF ");
#endif // DEBUG
      digitalWrite(PinCMD_MOTOR, bCmd);
}
//---------------------------------------------------------------------------
//  actions chaque seconde
//          chaque minute
//          chaque heure
//          chaque jour
//---------------------------------------------------------------------------
void fActions(void) {
    unsigned char iActionH, calc;
    boolean   bActionMn;
    //---------------------------------------------------------------------------
    if (FlagSeconde) {
       RTC.read(gTm);   //Récupère l'heure et la date courante
       lcd.setCursor(10, 0);
      // lcd.noBlink();
     //  lcd.noCursor();
        
        if ((gTm.Second % 2) > 0) lcd.print(F("x"));
        else                      lcd.print(F("+"));
        //---------------------------------------------------------------------------
        if (FlagMinute) {
           // chaque mn  calcul du max et min par heure
            TprNow =fRMesure();
            TprJMax = max(TprJMax, TprNow);
            TprJMin = min(TprJMin, TprNow);

            iActionH = pgm_read_byte(&tRemote[gT_Action.s.nProg].HH[gTm.Hour]);
            
            calc = round( pow( 2, (gTm.Minute / 15) )) ;
            
            bActionMn = ( (iActionH & calc ) <= 0 );
            show_DtH(bActionMn);  //  affiche la date et l'heure gTm en langage humain + temp + prog

            fActionMoteur(bActionMn);
            FlagMinute = false;
            //---------------------------------------------------------------------------
            if (FlagHeure) {
                //---------------------------------------------------------------------------
                  if (TprJMin < -3) {
                    // la temperature est trop froide pour attendre un changement de jour !
                    fQuelProg();
                }
                //---------------------------------------------------------------------------
                FlagHeure = false;
                //---------------------------------------------------------------------------
                if (FlagJour) {
                    Serial.println("\n Flag JOUR");
                    if (gT_Action.s.Auto=='A' ) fQuelProg();
                    DayRef = gTm.Day;
                    SauveDansRTC();  // adresse 0  sauve nProg
                    TprJMax = -99;
                    TprJMin = 99;
                    FlagJour = false;
                }
            }
        }
        
     }
}
//---------------------------------------------------------------------------*
//  MAJ des flags mn heure jour
//---------------------------------------------------------------------------
const  char cH[] = "<H>\n";
const  char cJ[] = "<J>\n";

void fSetFlags(void) {
    //---------------------------------------------------------------------------
    if (MnRef != gTm.Minute) {
        MnRef = gTm.Minute;
        FlagMinute = true;
        if (Href != gTm.Hour) {
            Href = gTm.Hour;
            FlagHeure = true;
            #ifdef DEBUG
            Serial.print(cH);
            #endif // DEBUG
            #ifdef SDREADER
            fwriteSD(cH);
            #endif // SDREADER
#ifdef DEBUGg
            Serial.print("\nDayRef ");
            Serial.print(DayRef);
            Serial.print("/ ");
            Serial.println(gTm.Day);
#endif

            if (DayRef != gTm.Day) {
                // chaque jour
                DayRef = gTm.Day;
                FlagJour = true;
                #ifdef DEBUG
                Serial.print(cJ);
                #endif // DEBUG
                #ifdef SDREADER
                fwriteSD(cJ);
                #endif // SDREADER

            }
        }
    }
}
///////////////////////////////////////////////////////////////////////////////
//initialize watchdog  2016 09 24
///////////////////////////////////////////////////////////////////////////////
#ifdef WD00

void WDT_Init()
{
    
      cli(); //disable interrupts
      // Clear MCU Status Register. Not really needed here as we don't need to know why the MCU got reset. page 44 of datasheet
      MCUSR = 0;
      // Disable and clear all Watchdog settings. Nice to get a clean slate when dealing with interrupts
      WDTCSR = (1<<WDCE)|(1<<WDE);
      //Start watchdog timer with 4s prescaller
      WDTCSR = (1 << WDIE) | (1 << WDE) | (1 << WDP3);
      sei(); //Enable global interrupts
     
      // ou 
      /*
    wdt_reset(); //reset watchdog
    wdt_disable();
    wdt_enable(WDTO_2S);
    */
}
#endif // WD00

/////////////////////////////////////////////////////////////////////////////////////
void setup(void) {
    byte res;
    pinMode(PinCMD_MOTOR, OUTPUT);
 
    #ifdef DEBUG
    Serial.begin(115200);
    while (!Serial); // wait until Arduino Serial Monitor opens
    #endif  
 
    lcd.begin(16, 2);   //Initialisation de l'écran

    res = fSetupCapteurTpr();

    #ifdef DEBUG
    if (!res) {
        Serial.println(F("\t\tTH OK"));
    }
    else {
        Serial.print(F("\t\tTH Err"));
        Serial.println(res);
    }
    #endif    
    lcd.clear();
    lcd.noBlink();
    lcd.noCursor();
    if (res) {
        lcd.setCursor(3, 0);
        lcd.print(F("TH Err"));
        lcd.print(res,10);
        while (true);
    }

    setSyncProvider(RTC.get);   // the function to get the time from the RTC
  RTC.readnvram(gT_Action.gRam, cSizeSAction, 0);

  // verification de validité de la rom

  if ( (gT_Action.s.t0 != 'J') || (gT_Action.s.t1 != 'l') || (gT_Action.s.f != 'f')) {
      // la ROM s'est dé-initialisée, on remet des valeurs par défaut
      gT_Action.s.t0 = 'J';
      gT_Action.s.t1 = 'l';
      gT_Action.s.nProg = 2;
      gT_Action.s.Auto = 'A';
      gT_Action.s.f    = 'f';

      // initialise avec  date et heure de compil 
      TimeElements te = StrDtToTm(__DATE__, __TIME__); // utilise tm
      RTC.write(te);   //  date heure
      SauveDansRTC();  // adresse 0  sauve nProg
#ifdef DEBUG
      Serial.println(F("Ré-init ROM "));
#endif

  }
  RTC.read(gTm);   //Récupère l'heure et le date courante
  gTmRef = gTm;
  show_DtH(0);
  
  DayRef = gTm.Day;
  Href   = gTm.Hour;
  MnRef  = gTm.Minute;

  TprNow =fRMesure();

  
  #ifdef WD00
  WDT_Init(); // jlp watchdog 2016 09 24
  #endif // WD00


#ifdef DEBUG
  Serial.print(F("size "));
  Serial.println(cSizeSAction,10);
#endif

#ifdef DEBUG
  Serial.println(F("\n"));
#endif    


}
////////////////////////////////////////////////////////////////////////////////////////////
void loop() { 
 //	Serial.println("     timer0>>>>>>>>>>"+ String(timer0,DEC) + "<<<<<<<<");
#ifdef WD00
  //  wdt_reset(); // jlp watchdog 2016 09 24
#endif  // WD00

    //---------------------------------------------------------------------------
    // mise à jour flag seconde
    //---------------------------------------------------------------------------
    if (millis() - previousMillis >= WAIT_1S) {
        previousMillis = millis();
        FlagSeconde = true;
        //---------------------------------------------------------------------------
        if (iRobot != A_SHOW_DATE) {
            Compteur10s += 1;
            #ifdef DEBUG
            Serial.print(F("Cpt10s= "));
            Serial.print(Compteur10s, DEC);
            Serial.print(F(" R"));
            Serial.println(iRobot, DEC);
            #endif
            // il ne se passe rien : remettre l'affichage sur date heure
            if (Compteur10s > 9) {
                Compteur10s = 0;
                iRobot = A_SHOW_DATE;
            }
        }
    }

  switch (iRobot) {
    case A_SHOW_DATE:
        // fonctionnement normal 
        // mise à jour des flag Mn H J
        // calcul des actions à faire
        fSetFlags();
        fActions();
        iRobot = fShowDate(iRobot);
        break;

    // fonctionnement paramétrage date et heure
    case Z_SET_JJ:
        iRobot = fSet_JJ(iRobot);
        break;
    case Z_SET_MM: 
        iRobot = fSet_MM(iRobot);
        break;
    case Z_SET_AA: 
        iRobot = fSet_AA(iRobot);
        break;
    case Z_SET_HH: 
        iRobot = fSet_HH(iRobot);
        break;
    case Z_SET_MN: 
        iRobot = fSet_MN(iRobot);
        break;
        // fonctionnement programme AUTO/Manuel
    case Z_SET_PG:
        iRobot = fSet_PG(iRobot);
        break;
    case Z_SET_AM:
        iRobot = fSet_AM(iRobot);
        break;
 
  }
}
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

