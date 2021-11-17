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
/////////////////////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------------------
//#undef DEBUG
#define DEBUG
//#define WD00  // watchdog 
#undef WD00  // watchdog 
#define myARDUINO_AVR_NANO
//#undef myARDUINO_AVR_NANO
//---------------------------------------------------------------------------------
#ifdef DEBUG
//#include "arduinotrace.h"
#endif // DEBUG

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>

//#include <TimeLib.h> fait par DS1307RTC.h
#include <OneWire.h> 
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t modifiée pour avoir la gestion de la ram
#include <LiquidCrystal.h>

//#include <elapsedMillis.h>
//#include <RTClib.h>   ne pas utiliser
//#include <dht.h>


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


// dht DHT1;
/* Broche du bus 1-Wire */
const byte BROCHE_ONEWIRE = PIN_A1;
byte bVal = 0;

/* Code de retour de la fonction getTemperature() */
enum DS18B20_RCODES {
    READ_OK,  // Lecture ok
    NO_SENSOR_FOUND,  // Pas de capteur
    INVALID_ADDRESS,  // Adresse reçue invalide
    INVALID_SENSOR  // Capteur invalide (pas un DS18B20)
};


/* Création de l'objet OneWire pour manipuler le bus 1-Wire */
OneWire ds(BROCHE_ONEWIRE);
byte data[9], addr[8];


//---------------------------------------------------------------------------------
typedef struct _sAction {
    char test[3];
    int nProg;
    char Auto;
} sAction;

#define lgNOMPROG 6
//---------------------------------------------------------------------------------
typedef  struct sR {
    char nomprog[lgNOMPROG+1];
    unsigned char UnJour[24] ;
} sR;

const int cSizeSAction = sizeof(sAction);

union _uAction {
  _sAction  s;
  byte gRam[cSizeSAction];
} uAction;

_uAction  gT_Action; 
/*
en fonction de la temperature et de l'heure, 
 ->  Hexa 4 bits qui correspondent  à chaque quart d'heure d'une heure 
     1 +2 +4 +8  l'heure entière
     4 + 8        la dernière demi heure
*/
#define MAXPROG 12
sR tRemote[MAXPROG+1] = {
    // HEURES ", {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 },
     "Arret "  , {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 0
     "Pg <-3"  , {  3, 0, 1, 3, 0,15, 5, 1, 1, 0, 1, 0, 0, 0, 3, 0, 1, 0, 1, 0, 1, 0,15, 1 }, // 1
     "Pg < 0"  , {  1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0 }, // 2
     "Pg <28"  , {  0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2 }, // 3
     "Pg 28 "  , {  0, 0, 0, 0, 0, 0, 0,15,15, 0, 0, 0,15,15, 1, 0, 0, 0,15,15,15,15, 0, 0 }, // 4
     "Pg <30"  , {  0, 0, 0, 0, 0, 0, 8,15,15, 0, 0, 0,15,15, 3, 0, 0, 0,15,15,15,15, 0, 0 }, // 5
     "Pg <32"  , {  0, 0, 0, 0, 0, 0,12,15,15, 0, 0, 0,15,15, 7, 0, 0, 0,15,15,15,15, 0, 0 }, // 6
     "Pg 32 "  , {  0, 0, 0, 0, 0, 0,14,15,15, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15,15, 0, 1 }, // 7
     "Pg 33 "  , {  0, 0, 0, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15,15, 0, 3 }, // 8
     "Pg 34 "  , {  0, 0, 0, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15,15, 0, 7 }, // 9
     "Pg >34"  , { 15, 0, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15, 0, 0, 0,15,15,15,15, 0,15, 0 }, // 10
     "Test x"  , { 15, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0 }, // 11
     "Marche"  , { 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 }  // 12
};


TimeElements gTm, gTmRef;
int lcd_key = 0;

bool  FlagMinute    = false;
bool  FlagHeure     = false;
bool  FlagJour      = false;
bool  FlagSeconde   = false;
bool  FlagGelFort   = false;
int   iWRAZ         = -1;
int   Compteur10s   = 0;
int   Compteur60s   = 0;
int   Compteur60mn = 0;
int iRobot = A_SHOW_DATE;
unsigned long previousMillis = 0;

int  TtprMinutes[60];
int  TtprHeures[24];

int  TprH = -99;
int  TprMaxJ = -99;
int  TprMinH = 100;

//---------------------------------------------------------------------------------
#ifdef myARDUINO_AVR_NANO 
//  SDA   D18 A4 (23)    
//  SCL   D19 A5 (24)

#define PIN_KBD  PIN_A0
#define PIN_CapteurTpr PIN_A1
#define PinCMD_MOTOR  PIN_A2


// LiquidCrystal( rs, enable,d4, d5, d6, d7);

LiquidCrystal lcd(PD6, PD7, PD2, PD3, PD4, PD5); //arduino NANO

#else
const int PinCMD_MOTOR = 7;
#define PIN_DHT01  PIN_A1
#define PIN_KBD    PIN_A0

//LiquidCrystal lcd(11, 12, 2, 3, 4, 5); // shield arduino UNO 
 LiquidCrystal lcd(8, 9, 4, 5, 6, 7); //// 
// LiquidCrystal lcd(12, 11, 5, 4, 3, 2); ////module nu photocopieuse 1 ligne

#endif // myARDUINO_AVR_NANO



/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------------------
//permet d'afficher le char °
//---------------------------------------------------------------------------------
String VersDegre(int n) {
  String res = tRemote[n].nomprog;

  if ( isdigit( tRemote[n].nomprog[res.length() - 1]) )
      res += String( (char) 223 );
 
  while (res.length() < lgNOMPROG+1)  res =res + " ";
  return res;
}
 //---------------------------------------------------------------------------------
//permet d'afficher les nombres sur deux chiffres
//---------------------------------------------------------------------------------
String Vers2Chiffres(byte nombre) {
    String resultat = "";
    if (nombre < 10)
        resultat = "0";
    return resultat += String(nombre, DEC);
}
/*
String Vers3Chiffres(int nombre) {
    String resultat = String(nombre, DEC);
    while ( resultat.length() < 4)  resultat = " " + resultat;
    return resultat;
}
*/
//---------------------------------------------------------------------------
// affiche la date et l'heure sur l'écran
//---------------------------------------------------------------------------
void show_DtH(TimeElements ltm) {
    int n;
     n = gT_Action.s.nProg;
     lcd.clear();
     lcd.noBlink();
     lcd.noCursor();
     // Date 
    String str = 
        Vers2Chiffres(ltm.Day) + "/" 
        +Vers2Chiffres(ltm.Month) + "/" 
        +tmYearToCalendar(ltm.Year) 
        +" " 
        +Vers2Chiffres(ltm.Hour) + ":" 
        +Vers2Chiffres(ltm.Minute);
    lcd.setCursor(0, 0);
    lcd.print(str);

    lcd.setCursor(0, 1); 
    if (TtprMinutes[gTm.Minute] >= 0)     str = String("+");
    else                                    str = String("");
    str += String(TtprMinutes[gTm.Minute]) + String((char)223);
    
    lcd.print(str);

    lcd.setCursor(5, 1);
    lcd.print(gT_Action.s.Auto);

    lcd.setCursor(9, 1);    
    lcd.print(VersDegre(n)); // sprintf pour degre 0x223'
}
//---------------------------------------------------------------------------
void SauveDansRTC(void)
{
    RTC.writenvram(0, gT_Action.gRam, cSizeSAction);
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
//---------------------------------------------------------------------------
//  toutes les heures
//  min et moyenne des mesures de temperatures par heure
//---------------------------------------------------------------------------
void  fTprCalcParHeure(int *tMin, int *tMoy )
{
    int somtpr = 0, j = 0;
    *tMin = 100;
    for (int i = 0; i < 60; i++) {
        if (TtprMinutes[i] != -99) {
            j++;
            somtpr += TtprMinutes[i];
            *tMin = min(*tMin, TtprMinutes[i]);
        }
    }
    *tMoy =(somtpr / j) ;
}


///////////////////////////////////////////////////////////////////////////////
byte fSetupCapteurTpr(void) {
    /* Reset le bus 1-Wire ci nécessaire (requis pour la lecture du premier capteur) */
    ds.reset_search();

    /* Recherche le prochain capteur 1-Wire disponible */
    if (!ds.search(addr)) {
        // Pas de capteur
        return NO_SENSOR_FOUND;
    }
    /* Vérifie que l'adresse a été correctement reçue */
    if (OneWire::crc8(addr, 7) != addr[7]) {
        return INVALID_ADDRESS;
    }
    /* Vérifie qu'il s'agit bien d'un DS18B20 */
    if (addr[0] != 0x28) {
        // Mauvais type de capteur
        return INVALID_SENSOR;
    }
    return READ_OK;
}
///////////////////////////////////////////////////////////////////////////////
// 
// Fonction de lecture de la température via un capteur DS18B20.
// 
///////////////////////////////////////////////////////////////////////////////
void getTemperature(float* temperature) {
    /* Reset le bus 1-Wire et sélectionne le capteur */
    ds.reset();
    ds.select(addr);

    /* Lance une prise de mesure de température et attend la fin de la mesure */
    ds.write(0x44, 1);
    // delay(800);

     /* Reset le bus 1-Wire, sélectionne le capteur et envoie une demande de lecture du scratchpad */
    ds.reset();
    ds.select(addr);
    ds.write(0xBE);

    /* Lecture du scratchpad */
    for (byte i = 0; i < 9; i++) data[i] = ds.read();
    /* Calcul de la température en degré Celsius */
    *temperature = ((data[1] << 8) | data[0]) * 0.0625;

}
//---------------------------------------------------------------------------
//  toutes les minutes
//  mesure de la temperature mise en tableau de 60 valeurs, 
//  renvoie la temperature mesurée
//---------------------------------------------------------------------------
int  fRMesure(void)
{
    int tpr;
    float temperature;
    for (int i = 0; i <= 5; i++) {
        getTemperature(&temperature);
        tpr = int(temperature);
        if ( (tpr > -50) && (tpr < 50)) {
            TtprMinutes[gTm.Minute] = tpr;
            break;
        }
        else {
            tpr = -99;
            delay(50);
            #ifdef DEBUG
            Serial.print(".");
            #endif // DEBUG
        }
    }
    return tpr;
};

/*
int  fRMesure(void)
{
    int tpr;
    DHT1.temperature = -99;
    DHT1.read(PIN_DHT01);
    tpr = int(DHT1.temperature);
    if (tpr > -50) {
        iTpr++;
        if (iTpr >= 60) iTpr = 0;
        Ttpr[iTpr] = tpr;
    }
    else {
        tpr = -99;
#ifdef DEBUG
        Serial.print(".");
#endif // DEBUG
    }
    return tpr;
};
int  fRMesure(void)
{
    int tpr;
    if (DHT1.read(PIN_DHT01) == DHTLIB_OK) {
        tpr = int(DHT1.temperature);
        iTpr++;
        if (iTpr >= 60) iTpr = 0;
        Ttpr[iTpr] = tpr;
        #ifdef DEBUG
        Serial.println(); // "                    temperature ");
       // Serial.print(tpr);
        #endif // DEBUG
    }
    else {
        tpr = -99;
        #ifdef DEBUG
        Serial.print(".");
       // Serial.print(DHT1.temperature);
        //Serial.print(".");
#endif // DEBUG
    }
    return tpr;
};

*/
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

#ifdef DEBUG
    Serial.println("read_LCD_buttons ");
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
        lcd.print(Vers2Chiffres(gTm.Day));
        lcd.setCursor(1, 0);
        break;
    }

    case btnDOWN: {
        gTm.Day -= 1;
        if (gTm.Day < 1 ) gTm.Day = 28;
        lcd.setCursor(0, 0);
        lcd.print(Vers2Chiffres(gTm.Day));
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
        lcd.print(Vers2Chiffres(gTm.Month));
        lcd.setCursor(4, 0);
        break;
    }

    case btnDOWN: {
        if (gTm.Month > 1) gTm.Month -= 1;
        lcd.setCursor(3, 0);
        lcd.print(Vers2Chiffres(gTm.Month));
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
#ifdef DEBUG
     //   Serial.println(Vers2Chiffres(gTm.Hour));
#endif // DEBUG

        lcd.print(Vers2Chiffres(gTm.Hour));
        lcd.setCursor(12, 0);
        break;
    }

    case btnDOWN: {
        if (gTm.Hour < 1) gTm.Hour = 23;
        else              gTm.Hour -= 1; 
#ifdef DEBUG
//        Serial.println(Vers2Chiffres(gTm.Hour));
#endif // DEBUG
        lcd.setCursor(11, 0);
        lcd.print(Vers2Chiffres(gTm.Hour));
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
        lcd.print(Vers2Chiffres(gTm.Minute));
        lcd.setCursor(15, 0);
        break;
    }

    case btnDOWN: {
        if (gTm.Minute < 1 ) gTm.Minute = 59;
        else                 gTm.Minute -= 1;
        lcd.setCursor(14, 0);
        lcd.print(Vers2Chiffres(gTm.Minute));
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
        lcd.print(VersDegre(gT_Action.s.nProg)); // sprintf pour degre 0x223'
        break;
   
    case btnDOWN: 
        gT_Action.s.nProg--;
        if (gT_Action.s.nProg < 0 )  gT_Action.s.nProg = MAXPROG;
        lcd.print(VersDegre(gT_Action.s.nProg)); // sprintf pour degre 0x223'
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
        FlagJour = true;  // init du programme
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
    tpr = TprMaxJ;
    if ( TprMinH < -3 )  gT_Action.s.nProg = 1;  // min plus petit que -3
    else if (tpr <= 0)   gT_Action.s.nProg = 2;  // plus petit que 0
    else if (tpr < 28)   gT_Action.s.nProg = 3;  // 0 < T < 28 
    else if (tpr == 28)  gT_Action.s.nProg = 4;  //  T = 28 
    else if (tpr < 31)   gT_Action.s.nProg = 5;  //  29 < T < 31 
    else if (tpr < 33)   gT_Action.s.nProg = 6;  //  31 < T < 33 
    else if (tpr == 32)  gT_Action.s.nProg = 7;  //  T = 32 
    else if (tpr == 33)  gT_Action.s.nProg = 8;  //  T = 33 
    else if (tpr == 34)  gT_Action.s.nProg = 9;  //  T = 34 
    else                 gT_Action.s.nProg = 10;  // > 34

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
        lcd.noBlink();
        lcd.noCursor();
        if ((gTm.Second % 2) > 0) lcd.print("x");
        else     lcd.print("+");
        //---------------------------------------------------------------------------
        if ( FlagMinute ) {
            int tprMn;

        // chaque mn  mise en tableau de la temperature 60 valeurs
            gTmRef.Minute = gTm.Minute;
            tprMn = fRMesure();
            show_DtH(gTm);  //affiche la date et l'heure en langage humain + temp + prog
            
            iActionH = tRemote[gT_Action.s.nProg].UnJour[gTm.Hour];
            calc = round( pow( 2, (gTm.Minute / 15) )) ;
            
            bActionMn = ( (iActionH & calc ) <= 0 );

            #ifdef DEBUG
            Serial.print("     H=");
            Serial.print(String(gTm.Hour, DEC));
            Serial.print(" ( iActionH = ");
            Serial.print(String(iActionH, DEC));
         
            Serial.print(")    mn= ");
            Serial.print(String(gTm.Minute, DEC));
            
            Serial.print(" ( bCMD = ");
            Serial.print(String( bActionMn ));
            Serial.print(")     Tpr = ");
            Serial.print(String(tprMn));

            Serial.print("              nProg = ");
            Serial.print(String(gT_Action.s.nProg));
           
            Serial.println(" ");
            #endif
            fActionMoteur(bActionMn);
            FlagMinute = false;
            //---------------------------------------------------------------------------
            if (FlagHeure) {
                int tprH;
                gTmRef.Hour = gTm.Hour;
                //---------------------------------------------------------------------------
                fTprCalcParHeure(&TprMinH, &tprH); // minima et moyenne par heure
                TprMaxJ = max(tprH, TprMaxJ);
                
                if (TprMinH < -3) {
                    // la temperature est trop froide pour attendre un changement de jour !
                    gT_Action.s.nProg = 1;
                }
                else {
                    //la temperature est remontée on va tester quel prog appliquer
                    if ( gT_Action.s.nProg == 1 ) 
                        if (gT_Action.s.Auto == 'A') fQuelProg();
                }
                //---------------------------------------------------------------------------
                FlagHeure = false;
                //---------------------------------------------------------------------------
                if (FlagJour) {
                    if (gT_Action.s.Auto=='A' ) fQuelProg();
                    gTmRef.Day = gTm.Day;
                    SauveDansRTC();  // adresse 0  sauve nProg
                    TprMaxJ = -99;
                    TprMinH = 100;
                    FlagJour = false;
                }
            }
        }
    }
}
//---------------------------------------------------------------------------*
//  MAJ des flags mn heure jour
//---------------------------------------------------------------------------
void fSetFlags(void) {
    //---------------------------------------------------------------------------
    if (Compteur60s > 59) {
        Compteur60s = 0;
        FlagMinute = true;
        Compteur60mn++;
        if (Compteur60mn > 59) {
            Compteur60mn =0;
            FlagHeure = true;
            #ifdef DEBUG
            Serial.println(" <<< FlagHeure  >>> ");
            #endif // DEBUG
            if (gTmRef.Day != gTm.Day) {
                // chaque jour
                FlagJour = true;
                #ifdef DEBUG
                Serial.println(" <<< FlagJour  >>> " );
                #endif // DEBUG
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
    /*
      cli(); //disable interrupts
      // Clear MCU Status Register. Not really needed here as we don't need to know why the MCU got reset. page 44 of datasheet
      MCUSR = 0;
      // Disable and clear all Watchdog settings. Nice to get a clean slate when dealing with interrupts
      WDTCSR = (1<<WDCE)|(1<<WDE);
      //Start watchdog timer with 4s prescaller
      WDTCSR = (1 << WDIE) | (1 << WDE) | (1 << WDP3);
      sei(); //Enable global interrupts
      */
      // ou   
    wdt_reset(); //reset watchdog
    wdt_disable();
    wdt_enable(WDTO_2S);
}
#endif // WD00

/////////////////////////////////////////////////////////////////////////////////////
void setup(void) {
    byte res;

    pinMode(PinCMD_MOTOR, OUTPUT);
 
#ifdef DEBUG
	Serial.begin(115200);
    while (!Serial); // wait until Arduino Serial Monitor opens
    Serial.println("--debut--");
#endif    
 
  lcd.begin(16,2);   //Initialisation de l'écran
  do {
      res = fSetupCapteurTpr();
      String str = " Verif du capteur :  "
          + String(res)
          + F("  ( 0=OK,  1=Pas de capteur, 2=Adresse reçue invalide, 3=pas un DS18B20");

#ifdef DEBUG
      Serial.println(str);
#endif    
      lcd.clear();
      lcd.noBlink();
      lcd.noCursor();
      lcd.setCursor(0, 0);
      lcd.print(str);

  } while (res != READ_OK);

  for (int i = 0; i < 60; i++)   TtprMinutes[i] = -99;
  for (int i = 0; i < 24; i++)   TtprHeures[i] = -99;

   setSyncProvider(RTC.get);   // the function to get the time from the RTC
  RTC.readnvram(gT_Action.gRam, cSizeSAction, 0);

#ifdef DEBUG
  Serial.println(" ****************************** ");
#endif    

  // verification de validité de la rom

  if ( !(  (gT_Action.s.test[0] == 'J')
        && (gT_Action.s.test[1] == '-')
        && (gT_Action.s.test[2] == 'L') )
      ) {
      // la ROM s'est dé-initialisée, on remet des valeurs par défaut
      gT_Action.s.test[0] = 'J';
      gT_Action.s.test[1] = '-';
      gT_Action.s.test[2] = 'L';
      gT_Action.s.test[3] = 0;
      gT_Action.s.nProg = 2;
      gT_Action.s.Auto = 'A';

      // initialise avec  date et heure de compil 
      TimeElements te = StrDtToTm(__DATE__, __TIME__); // utilise tm
      RTC.write(te);   //  date heure
      SauveDansRTC();  // adresse 0  sauve nProg
#ifdef DEBUG
      Serial.print("init valeurs par défaut [ ");
      Serial.print(gT_Action.s.test);
      Serial.println(" ]");
#endif

  }
  RTC.read(gTm);   //Récupère l'heure et le date courante
  gTmRef = gTm;
  show_DtH(gTm);

  Compteur60s  = gTm.Second;
  Compteur60mn = gTm.Minute;

  TtprMinutes[gTm.Minute] =fRMesure();

  
  #ifdef WD00
  WDT_Init(); // jlp watchdog 2016 09 24
  #endif // WD00


#ifdef DEBUG
  char sDebug[80];
  sprintf(sDebug, " taille struc sAction %u", cSizeSAction);
  Serial.println(sDebug);

  Serial.println("--Fin Setup--");
#endif
/*
  for (int i = 0; i < 5; i++) {
      Serial.print("  2 puissance ");
      Serial.print(String(i, DEC));
      Serial.print("  =  ");
      
      c2 = round( pow(2, i) );
      Serial.print(String(c2, DEC) );
      Serial.println("");

  }*/

  FlagJour = true; // positionne le prog en fonction de la temperature

}
////////////////////////////////////////////////////////////////////////////////////////////
void loop(){
//	Serial.println("     timer0>>>>>>>>>>"+ String(timer0,DEC) + "<<<<<<<<");
#ifdef WD00
    wdt_reset(); // jlp watchdog 2016 09 24
#endif  // WD00

    //---------------------------------------------------------------------------
    // mise à jour flag seconde
    //---------------------------------------------------------------------------
    if (millis() - previousMillis >= WAIT_1S) {
        previousMillis = millis();
        Compteur60s += 1;
        FlagSeconde = true;
        //---------------------------------------------------------------------------
        if (iRobot != A_SHOW_DATE) {
            Compteur10s += 1;
            #ifdef DEBUG
            Serial.print(" Compteur10s= ");
            Serial.println(String(Compteur10s, DEC));
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



