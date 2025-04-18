#include <Arduino.h>

#define ASSERT(exp) ((exp) ?: Serial.printf("%s:%u: assertion %s failed\n", __FILENAME__, __LINE__, #exp))

#define PWM_MAX ((1<<10)-1)/*1023*/

#define PULSO_MIN 1000
#define PULSO_MAX 2000
#define PULSO_DIF (PULSO_MAX-PULSO_MIN)
#define PULSO_MED (PULSO_MIN + PULSO_DIF/2)

#define TEMPO_FOGO 2000

#define PROTO
// #define DRACARYS

#if defined(PROTO)
  #define MIXAR
  #undef FOGO_MANUAL

  #define eixo_x_ch 9 /*ch1?*/
  #define eixo_y_ch 10/*ch3?*/
  #define fogo_ch   8 /*ch5*/

  #define roda_esq_m1 1
  #define roda_esq_m2 4

  #define roda_dir_m1 1
  #define roda_dir_m2 4

  #define fogo_m1 2
  #define fogo_m2 3
  #define isqueiro_fogo ()
#elif defined(DRACARYS)
  #undef MIXAR
  #undef FOGO_MANUAL

  #define eixo_x_ch 10/*ch1*/
  #define eixo_y_ch 8 /*ch3*/
  #define fogo_ch   9 /*ch5*/

  #define roda_esq_m1 4
  #define roda_esq_m2 3

  #define roda_dir_m1 1
  #define roda_dir_m2 2

  #define fogo_m1 6
  #define fogo_m2 7
  #define isqueiro_fogo ()
#endif

struct par {
    union { int16_t esq, x, a; };
    union { int16_t dir, y, b; };
};
enum estado_fogo { PARADO, INDO, VOLTANDO };
char* estado_fogo_str[] = {
    [PARADO]="PARADO", [INDO]="INDO", [VOLTANDO]="VOLTANDO",
};

enum estado_fogo fogo = PARADO;
unsigned long fim_fogo = 0;

void setup() {
    pinMode(eixo_x_ch, INPUT);
    pinMode(eixo_y_ch, INPUT);
    pinMode(fogo_ch,   INPUT);
    
    pinMode(roda_esq_m1, OUTPUT);
    pinMode(roda_esq_m2, OUTPUT);
    pinMode(roda_dir_m1, OUTPUT);
    pinMode(roda_dir_m2, OUTPUT);

    pinMode(fogo_m1, OUTPUT);
    pinMode(fogo_m2, OUTPUT);

    Serial.begin(115200);
}

void loop() {
    // lê o que o rádio manda como pulsos
    unsigned long pulso_fogo = pulseIn(fogo_ch,   HIGH, 20000);
    unsigned long pulso_x    = pulseIn(eixo_x_ch, HIGH, 20000);
    unsigned long pulso_y    = pulseIn(eixo_y_ch, HIGH, 20000);

    // failsafe // checa qualquer canal pra ver se não teve timeout
    if (pulso_fogo == 0) {
        failsafe(); return;
    }

    // arma
    static bool fogo_anterior = false;
    bool pedido_fogo = pulso_fogo > PULSO_MED;
  #if !defined(FOGO_MANUAL)
    switch (fogo) {
        case PARADO: {
            if (pedido_fogo != fogo_anterior) {
                fim_fogo = millis() + TEMPO_FOGO;
                fogo_anterior = pedido_fogo;

                fogo = pedido_fogo ? INDO : VOLTANDO;
                if (pedido_fogo) motor_fogo( PWM_MAX);
                else             motor_fogo(-PWM_MAX);
            }
        } break;
        case INDO: case VOLTANDO: {
            if (acabou_fogo()){
                motor_fogo(0);
                fogo = PARADO;
            }
        } break;
    }
  #else
    if      (pulso_fogo > (PULSO_MED+PULSO_DIF/3)) motor_fogo( PWM_MAX/3);
    else if (pulso_fogo < (PULSO_MED-PULSO_DIF/3)) motor_fogo(-PWM_MAX/3);
    else                                           motor_fogo(0);
  #endif
    // movimento
    struct par vels = mixar(pulsoPWM(pulso_x),
                            pulsoPWM(pulso_y));
    mover(vels.esq, vels.dir);

    //! debug
    Serial.printf("%4lu, %04lu:\t" "pwm %5d, %5d | "  "%d, "       "fogo=%s\n",
                  pulso_x,pulso_y, vels.esq,vels.dir, pedido_fogo, estado_fogo_str[fogo]);
}

void failsafe() {
    Serial.println("tentando failsafe");

    // if (fogo == LIGANDO) while (!acabou_fogo());
    // if (fogo == LIGADO)  parar_fogo();
    // while (!acabou_fogo());

    motor_fogo(0); //! errado!!!

    mover(0, 0);
}

bool acabou_fogo() { return millis() > fim_fogo; }
void motor_fogo(int16_t vel) {
    motor(fogo_m1,fogo_m2, vel);
}

void mover(int16_t esq, int16_t dir) {
    motor(roda_esq_m1, roda_esq_m2, esq);
    motor(roda_dir_m1, roda_dir_m2, dir);
}

void motor(uint8_t m1, uint8_t m2, int16_t vel) {
    if (vel < 0) {
        analogWrite(m1, abs(vel));
        analogWrite(m2, 0);
    } else {
        analogWrite(m1, 0);
        analogWrite(m2, vel);
    }
}

struct par mixar(int16_t x, int16_t y) {
  #ifdef MIXAR
    //! avaliar a ideia de normalizar esse "vetor" nos cantos
    return {
        constrain(y + x, -PWM_MAX,PWM_MAX),
        constrain(y - x, -PWM_MAX,PWM_MAX),
    };
  #else
    return { x, y };
  #endif
}

int16_t pulsoPWM(unsigned long pulso) {
    int16_t pwm = map(pulso, PULSO_MIN,PULSO_MAX,
                            -PWM_MAX,  PWM_MAX);
    return constrain(pwm, -PWM_MAX,PWM_MAX);
}
