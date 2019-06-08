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

#include <util/delay.h>
#include <IRremote.h>

#define BITRATE 500                          // Bitrate en bps
#define TX 3                                 // Puerto digital para la transmision de datos

long time_interval = 1000000 / BITRATE / 2 ; // Intervalo de tiempo entre los bits manchester

int sequence = 0;

IRrecv irrecv(11);                           // Objeto que manejará el receptor infrarrojos localizado
decode_results results;                      // en el pin digital 11

// Funcion que trasforma un entero a una cadena de bits de una longitud
// determinada, indicada por el parametro s. Para ello emplea la funcion
// bitRead(entero, pos) y suma el resultado a una string

String int_to_bits(int integer, int s) {
  String bits = "";
  int len = bits.length();
  for(int i = s - 1; i >= 0; i--) {
    bits += bitRead(integer, i);
  }
  return bits;
}

// Funcion que toma una cadena de bits como entrada y produce
// un entero como salida

int bits_to_int(String bits) {
  int value = 0;
  for (int i = 0; i < bits.length(); i++) {
    value *= 2;                            // En binario cada posicion es el doble que la anterior
    if (bits.charAt(i) == '1') value++;    // Si hay  un uno, suma uno a value
  }
  return value;
}

// Funcion necesaria para transmitir los datos codificados en Manchester

void transmit(String data) {
  for(int i = 0; i <= data.length(); i++) {
    if(data[i] == '0') {                  // Si la señal es un 0, emitir
      digitalWrite(TX, LOW);              // 0 - 1
      _delay_us(time_interval);
      digitalWrite(TX, HIGH);
      _delay_us(time_interval);
    }
    if(data[i] == '1') {                  // Si la señal es un 1, emitir
      digitalWrite(TX, HIGH);             // 1 - 0
      _delay_us(time_interval);
      digitalWrite(TX, LOW);
      _delay_us(time_interval);
    }
  }
}

// Funcion que transmite el preambulo (10 unos)

void preamble() {
  transmit("1111111111");
}

void setup() {
  pinMode(TX, OUTPUT);
  Serial.begin(115200);   // Inicializar comunicacion serial a 115200 baudios
  irrecv.enableIRIn();    // Inicializar el modulo de tranmision de infrarrojos
}

void loop() {
  TIMER_DISABLE_INTR;     // Desabilitar las interrupciones de la funcion enableIRIn()
  
  int new_package = 0;
  byte sum = 0;
  
  String data[16];
  String seq = int_to_bits(sequence, 10);  // Almacenar el numero de secuencia en binario
  String checksum;

  for(int i = 0; i < 15; i++) {
    data[i] = int_to_bits(0, 8);           // Inicializar el array de datos con 0 (por si no se llena,
  }                                        // que el resto sean ceros
  
  for(int i = 0; i < 16; i++) {            // Bucle para leer hasta 16 caracteres
    if(Serial.available()) {               // Si se han recibido datos
      char data_read = Serial.read();      // Leer dato
      Serial.print(data_read);
      
      if(data_read == 10) {                    // Si se encuentra el caracter de salto de linea
        data[15] = int_to_bits(data_read, 8);  // Insertar el caracter al final del array
        break;                                 // Salir del bucle de llenado de caracter ya que el usuario ha pulsado enter
      }
      
      data[i] = int_to_bits(data_read, 8);     // Llenar la posicion i con el elemento i-esimo
    }
    else {                                     // No se ha recibido caracter
      transmit("101010");                      // Transmitir cadena de "idle"
      return;                                  // Salir del bucle loop y volver a intentar recuperar datos
    }
  }

  for(int i = 0; i < 16; i++) {
    sum += bits_to_int(data[i]);               // Realizar la suma checksum
  }

  checksum = int_to_bits(sum, 8);              // Transformar la suma a cadena de bits

  do {                                         // Transmitir el paquete al menos una vez
    transmit("101010");                        // Es necesario un ciclo "idle" para que el receptor detecte el preambulo
    
    preamble();                                // Enviar preambulo
    
    transmit("1");                             // Separador
    transmit(seq);                             // Enviar numero de secuencia
  
    for(int i = 0; i < 16; i++) {              // Enviar los 16 caracteres
      transmit("1");                           // Separador
      transmit(data[i]);                       // Dato i-esimo
    }
    
    transmit("1");                             // Separador
    transmit(checksum);                        // Enviar checksum
    
    transmit("0");                             // Enviar terminador de paquete

    TIMER_ENABLE_INTR;                         // Reiniciar la funcion enableIRIn()
    irrecv.decode(&results);                   // Recibir la confirmacion del receptor
  } while(!results.value);                     // Reenviar el paquete hasta que no se encuentre confirmacion

  TIMER_DISABLE_INTR;                          // Desactivar el modulo infrarrojo
  
  delay(30);

  sequence++;                                  // Aumentar el numero de secuencia en uno
  if(sequence >= 1023) sequence = 0;           // Si el numero de secuencia esta a punto de rebasar, reinicia la cuenta
}
