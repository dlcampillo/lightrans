//  MIT License
//
//  Copyright (c) 2018 David Lozano Campillo
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.

#include <util/delay.h> // Biblioteca necesaria para emplear _delay_us()
#include <IRremote.h>   // Biblioteca necesaria para la comunicación infrarroja

#define BAUDRATE 2000000 // Baudrate en baudps
#define RX A0 // Puerto receptor

int const preamble[20] = {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0};
int buffer_preamble[20];

int val, flag;

IRsend irsend;

// Función que toma una cadena de bits como entrada
// y devuelve el número entero correspondiente

int bits_to_int(String bits) {
  int value = 0;
  for (int i = 0; i < bits.length(); i++) {
    value *= 2;
    if (bits.charAt(i) == '1') value++;
  }
  return value;
}

// Función requerida para demodular la señal manchester
// Como se puede apreciar, la señal aparece invertida, por
// lo que 1 es 01 y 0 es 10

String demod_manchester(String string, int len) {
  String data_demod = "";
  for(int i = 0; i < len * 2; i += 2) {
    if(string.charAt(i) == '1') {
      data_demod += "0"; 
    }
    if(string.charAt(i) == '0') {
      data_demod += "1";
    }
  }
  return data_demod;
}

// Deteccion de la señal: Se detecta un cambio de la señal, el tiempo
// que ha llevado ese cambio y si se trata de 1, 0, 11 o 00. Debido a que
// sería costoso crear una variable para cada dato recogido, se ha
// decidido por utilizar un puntero reutilizable en vez de una variable global

void get_data(int *val, int *flag) {
  int v = digitalRead(RX);
  int dv = digitalRead(RX);
  long t = micros();
  
  while(v == dv) {
    dv = digitalRead(RX);
  }

  // Si v es diferente de dv se ha producido un cambio de señal,
  // por lo que calculamos el tiempo que ha transcurrido
  
  int dt = micros() - t;


  if(dt < 1000) {
    *flag = 0; // 1 o 0
  }
  if(dt >= 1200 && dt < 3000) {
    *flag = 1; // 11 o 00
  }

  _delay_us(100);
    
  *val = v;
}

// Función que "escucha" una cadena de cierta longitud y la devuelve

String get_chain(int len) {
  String data = "";
  
  for(int i = 0; i < (len * 2 + 1); i++) { // Multiplicamos la longitud por dos ya que la señal
    get_data(&val, &flag);                 // viene codificada en manchester
    if(flag == 1) {
      data += String(val);
      i++;
    }
    data += String(val);
  }

  _delay_us(1000);                         // Como se envia un "1" como espaciador entre caracter y
                                           // carácter que tarda 1 ms, esperamos dicho tiempo
  return data;
}

// Detección del preámbulo: Se crea un buffer circular para
// compararlo con la cadena del preámbulo

int detect_preamble() {
  get_data(&val, &flag);
  int check = 1;

  if(flag == 0) {
    for(int i = 0; i < 19; i++) {
      buffer_preamble[i] = buffer_preamble[i+1]; // Si solo es 1 o 0 desplazar 1
    }
  }
  else {
    for(int i = 0; i < 18; i++) {
      buffer_preamble[i] = buffer_preamble[i+2]; // Si es 11 o 00 desplazar 2
    }
    buffer_preamble[18] = val;
  }
  buffer_preamble[19] = val;
  
  for(int i = 0; i < 20; i++) {
    if(buffer_preamble[i] != preamble[i]) {
      check = 0;                                 // Si un carácter no coincide con el preambulo, no es este
    }
  }

  _delay_us(500);

  return check;
}

void setup() {
  pinMode(RX, INPUT);               // Inicializar el pin RX (A0) para escuchar datos
  Serial.begin(BAUDRATE);           // Iniciar comunicacion serial a BAUDRATE (2000000) baudios
  
  for(int i = 0; i < 20; i++) {
    buffer_preamble[i] = 0;         // Llenar el buffer circular que se empleara en la deteccion del preambulo
  }
}

void loop() {
  if(detect_preamble()) {           // Si se ha detectado un preambulo
    _delay_us(500);
    String seq = get_chain(10);   // Obtener el numero de secuencia
    String data[16];
    String checksum;
    byte sum = 0;
    
    for(int i = 0; i < 16; i++) {
      data[i] = get_chain(8);     // Obtener 16 caracteres de 8 bits cada uno
    }

    checksum = get_chain(8);      // Obtener el checksum

    for(int i = 0; i < 16; i++) {
      sum += bits_to_int(demod_manchester(data[i], 8)); // Realizar la suma de comprobacion de los datos obtenidos
    }
    
    if(bits_to_int(demod_manchester(checksum, 8)) == sum) {
      for(int i = 0; i < 16; i++) {
        Serial.print(char(bits_to_int(demod_manchester(data[i], 8)))); // Imprimir los caracteres obtenidos en el monitor serie
      }
      irsend.sendNEC(0xFF, 8);    // Enviar señal de confirmación al emisor
    }
  }
}
