/*
  Battery Analyzer - InspirAR Health Tech v1.0.
  Created by Gabriel Rosa, February 3, 2022.
  All rights reserved.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SERIAL_BAUD_RATE 38400UL

#define BATTERY_PIN A0

#define GREEN_LED A11
#define BLUE_LED A9
#define RED_LED A10
#define LED_VCC A8

#define RELAY_VCC 8
#define RELAY_GND 9
#define RELAY_PIN 10

#define ACS_VCC 53
#define ACS_GND 52
#define ACS_PIN A14

#define LCD_VCC 23
#define LCD_GND 22
#define LCD_SDA 20
#define LCD_SCL 21

#define col 16
#define lin 2
#define ende 0x27

const float load = 1.3;

const float r1 = 30000.0;
const float r2 = 15000.0;

const float ACSoffset = 2250.0;
const float mVperAmp = 100;

const float samples = 3.0;

bool flag_medicaoRealizada = false;
bool flag_habilitaPrograma = false;

unsigned long blinkLed = millis();
unsigned long timeout = millis();

LiquidCrystal_I2C lcd(ende, col, lin);

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);

    // Configuração do pino para a leitura analogica da tensao da bateria
    pinMode(BATTERY_PIN, INPUT);

    // Configuração dos pinos do LED
    pinMode(GREEN_LED, OUTPUT);
    pinMode(BLUE_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(LED_VCC, OUTPUT);

    // Configuração dos pinos do relé
    pinMode(RELAY_VCC, OUTPUT);
    pinMode(RELAY_GND, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);

    // Configuracao dos pinos do sensor de corrente
    pinMode(ACS_VCC, OUTPUT);
    pinMode(ACS_GND, OUTPUT);
    pinMode(ACS_PIN, INPUT);

    // Configuracao dos pinos do display LCD
    pinMode(LCD_GND, OUTPUT);
    pinMode(LCD_VCC, OUTPUT);

    // Alimentacao do sensor de corrente
    digitalWrite(ACS_GND, LOW);
    digitalWrite(ACS_VCC, HIGH);

    // Alimentacao do rele
    digitalWrite(RELAY_GND, LOW);
    digitalWrite(RELAY_VCC, HIGH);
    digitalWrite(RELAY_PIN, HIGH);

    // Alimentacao do led
    digitalWrite(LED_VCC, HIGH);

    // Alimentacao do LCD
    digitalWrite(LCD_VCC, HIGH);
    digitalWrite(LCD_GND, LOW);

    // Reset dos led
    toggle_leds(true, false, false);

    // Configuracao do LCD
    lcd.init();
    lcd.backlight();
    lcd.clear();

    flag_habilitaPrograma = true;

    waitBattery();

    // MENU PARA DEBUG
    Serial.println(" ");
    Serial.println("MENU");
    Serial.println("1 - Calcular a corrente");
    Serial.println("2 - Procurar endereços I2C");
    Serial.println("3 - Habilitar programa");
    Serial.println("4 - Escreve no display");
}

void loop()
{
    if (flag_habilitaPrograma)
    {
        if (flag_medicaoRealizada)
        {
            // Timeout de 1 minuto para resfriamento das resistências
            if ((millis() - timeout) > 60000)
            {
                flag_medicaoRealizada = false;
            }
        }
        else if (analogRead(BATTERY_PIN) < 500 && !flag_medicaoRealizada)
        {
            waitBattery();
        }
        else if (analogRead(BATTERY_PIN) > 500 && !flag_medicaoRealizada)
        {
            execMeasurement();
        }
    }
    else
    {
        lcd.setCursor(0, 0);
        lcd.print("EXECUTANDO DEBUG");

        if (Serial.available())
        {
            char c = Serial.read();

            switch (c)
            {
            case '1':
                calculaCorrente();
                break;

            case '2':
                findAddressI2C();
                break;

            case '3':
                flag_habilitaPrograma = true;
                break;

            case '4':
                testarDisplay();
                break;

            default:
                break;
            }
        }
    }
}

void toggle_leds(bool red, bool green, bool blue)
{
    (green == true) ? digitalWrite(GREEN_LED, LOW) : digitalWrite(GREEN_LED, HIGH);
    (blue == true) ? digitalWrite(BLUE_LED, LOW) : digitalWrite(BLUE_LED, HIGH);
    (red == true) ? digitalWrite(RED_LED, LOW) : digitalWrite(RED_LED, HIGH);
}

void waitBattery()
{
    toggle_leds(true, false, false);
    delay(500);
    toggle_leds(false, false, false);
    delay(500);

    Serial.println("Aguardando a bateria");

    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("CONECTE A");
    lcd.setCursor(5, 1);
    lcd.print("BATERIA");
}

void stateBattery(float meanResistance)
{
    if (meanResistance < 0.5)
    {
        toggle_leds(false, true, false);
    }
    else if (meanResistance < 1.0)
    {
        toggle_leds(true, true, false);
    }
    else
    {
        toggle_leds(true, false, false);
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RESISTENCIA: " + String(meanResistance));
    lcd.setCursor(0, 1);
    lcd.print("TIMEOUT DE 1 MIN");

    Serial.println();
    Serial.println("Resultado da bateria");
}

void execMeasurement()
{
    float accumulate_resistance = 0.0;

    // Inicio da medição
    toggle_leds(false, false, true);

    Serial.println("Medição em andamento");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MEDICAO INICIADA");

    delay(2000);

    lcd.clear();

    for (int i = 0; i < samples; i++)
    {
        float voltage_open = 0.0;
        float voltage_load = 0.0;
        float current_load = 0.0;
        float resistance = 0.0;
        float current_acs = 0.0;

        float accumulate_voltage_open = 0.0;
        float accumulate_voltage_load = 0.0;
        float accumlate_current_acs = 0.0;

        lcd.setCursor(0, 0);
        lcd.print(String(i + 1) + "/" + String(int(samples)));

        // Aquisição da tensao da bateria sem carga
        delay(5000);
        for (int i = 0; i < 10; i++)
        {
            accumulate_voltage_open += analogRead(BATTERY_PIN) * 0.01466;
            delay(50);
        }

        // Se a bateria foi retirada, a rotina é paralisada
        if (analogRead(BATTERY_PIN) < 200)
        {
            digitalWrite(RELAY_PIN, HIGH);
            flag_medicaoRealizada = true;
            timeout = millis();

            Serial.println("Timeout de 1 minuto");
            Serial.println();

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.println("BAT DESCONECTADA");
            lcd.setCursor(1, 1);
            lcd.println("TIMEOUT 1 MIN");

            return;
        }

        
        digitalWrite(RELAY_PIN, LOW);
        
        // Aquisição da tensão e da corrente da bateria com carga
        delay(2000);
        for (int i = 0; i < 10; i++)
        {
            accumulate_voltage_load += analogRead(BATTERY_PIN) * 0.01466;

            accumlate_current_acs += (((analogRead(ACS_PIN) * 0.004882) - 2.4)) / 0.1;

            delay(50);
        }
        
        digitalWrite(RELAY_PIN, HIGH);

        // Se a bateria foi retirada, a rotina é paralisada
        if (analogRead(BATTERY_PIN) < 200)
        {
            digitalWrite(RELAY_PIN, HIGH);
            flag_medicaoRealizada = true;
            timeout = millis();

            Serial.println("Timeout de 1 minuto");
            Serial.println();

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.println("BAT DESCONECTADA");
            lcd.setCursor(1, 1);
            lcd.println("TIMEOUT 1 MIN");

            return;
        }

        voltage_open = accumulate_voltage_open * 0.1;
        voltage_load = accumulate_voltage_load * 0.1;
        current_load = accumlate_current_acs * 0.1;

        resistance = (voltage_open - voltage_load) / current_load;
        accumulate_resistance += resistance;

        Serial.println("Medicao: " + String(i) + " Vopen: " + String(voltage_open) + " VLoad: " +
                       String(voltage_load) + " Current: " + String(current_load) +
                       " Resistance: " + String(resistance));

        lcd.setCursor(10, 0);
        lcd.print("R:" + String(resistance));

        lcd.setCursor(0, 1);
        lcd.print(String(voltage_open, 1) + "V " + String(voltage_load, 1) + "V " + String(current_load, 1) + "A");
    }

    float meanResistance = accumulate_resistance / samples;

    Serial.println("Resistencia Media: " + String(meanResistance));

    flag_medicaoRealizada = true;

    timeout = millis();

    Serial.println("Timeout de 1 minuto");
    Serial.println();

    stateBattery(meanResistance);
}

void calculaCorrente()
{
    float acsValue = 0.0;
    float samples = 0.0;
    float avgAcs = 0.0;
    float avgValueF = 0.0;

    for (int i = 0; i < 150; i++)
    {
        acsValue = analogRead(ACS_PIN);
        samples = samples + acsValue;
        delay(3);
    }

    avgAcs = samples / 150.0;

    avgValueF = ((avgAcs * (5.0 / 1024.0)) - 2.4) / 0.1;

    Serial.println(avgValueF);
}

void findAddressI2C()
{
    Wire.begin();

    Serial.println("I2C Scanner");

    byte error, address;
    int nDevices;
    nDevices = 0;
    for (address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0)
        {
            Serial.print("Endereço I2C encontrado: 0x");
            if (address < 16)
                Serial.print("0 ");
            Serial.println(address, HEX);

            nDevices++;
        }
        else if (error == 4)
        {
            Serial.print("ERRO ");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);
        }
    }
    if (nDevices == 0)
        Serial.println("Nenhum endereço i2C encontrado ");
    else

        Serial.println(" Feito !");

    delay(5000);
}

void testarDisplay()
{
    lcd.setCursor(0, 0);
    lcd.print("TESTE");
}