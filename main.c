/*
 * GccApplication4.c
 *
 * Created: 27/11/2017 14:02:30
 * Author : EDZÉ
 */ 

#define F_CPU 1000000UL
#include<avr/io.h>
#include<stdio.h>
#include<avr/interrupt.h>
#include<avr/io.h>
#include<util/delay.h>

void init(void);
void lerad(void);
void inversao(void);
void usom(void);

int flag_led=0;
int timer=100;

volatile char flag_MEIO=0;
volatile char flag_ESQ=0;
volatile char flag_DIR=0;
volatile char flag_entroncamento=0;
volatile char flag_tudozero=0;
volatile char flag_usom=0;
volatile int estado;
volatile uint8_t S_M;
volatile uint8_t S_ESQ;
volatile uint8_t S_DIR;
volatile long int TimerOverflow;
volatile long int count;
volatile double distance;

void init(void){
	
	TCCR0A=0b00000010;		//	CTC, Normal port operation, OC0A disconnected
	TCCR0B=0b00000011;		//	Modo CTC, Prescaler=64, interrupção gerada de 5 em 5 ms
	OCR0A=77;				//	OCR0A=78, 78-1=77, ficando T=10ms e f=100hz
	TIMSK0=0b00000010;		//	Activa a interrupção do TC0
	TCCR2A|=0b10100001;		//	Clear OC2A when up-counting, set when down-counting
	TCCR2B|=0b00000110;		//	PWM phase correct, prescaler=8
	DDRB=0b01001000;		//	Define PB3 e PB6 como saídas
	PORTB=0b00001000;			
	DDRC=0b00110000;		//	Define PC4 e PC5 como saídas
	DDRD=0b00001011;		//	Define PD0, PD1 e PD3 como saídas
	PORTD=0b00001001;
	ADCSRA|=0b10000110;		//	ADEN=1: liga o conversor||fator divisao 64
	ADMUX=0b01100000;		//	Liga o ADC0
	TIMSK1=(1 << TOIE1);	//	TOIE1 a 1
	TIFR1=(1<<TOV1);		//	TOV1 a 1
	TCCR1A = 0;				//	Set all bit to zero Normal operation
	sei();					//	Activa as interrupções globais, coloca a 1 o bit 7 de SREG
}

ISR(TIMER0_COMPA_vect){			//Interrupção do TC0 compare match A
	timer--;					//Decrementa a variavel
	if(timer==0){				//Condição, se for igual a zero executa o codigo entre parenteses
		flag_usom=1;
			switch (flag_led){
			case 0:				//Caso a flag_on esteja a 0, mete-a a 1.
				PORTC=0b00100000;
				flag_led=1;
				break;
			case 1:				//Caso a flag_on esteja a 1, mete-a a 0.
				PORTC=0b00000000;
				flag_led=0;
				break;
			default:
				break;
		}
		timer=100;				//Recarrega a variavel com o valor 100, 100*5ms=500ms
	}
}

ISR(TIMER1_OVF_vect){
	TimerOverflow++;			//	Incrementa um valor a cada ciclo de clock(1us)
}


void usom(void){
	
	PORTB |= (1 << PB6);						// Valor lógico de PB6 a 1
	_delay_us(10);								// Duração do pulso do trigger(10us)
	PORTB &= (~(1 <<PB6));						// Valor lógico de PB6 a 0
	
	TCNT1 = 0;									// Limpa o Timer Counter
	TCCR1B = 0x41;								// Trigger ascendente
	TIFR1 = 1<<ICF1;							// Coloca a flag ICP a 0
	TIFR1 = 1<<TOV1;							// Coloca a flag do TimerOverflow a 0

	//Calculate width of Echo by Input Capture (ICP)
	
	while ((TIFR1 & (1 << ICF1)) == 0);			// Espera pelo flanco ascendente
	TCNT1 = 0;									// Limpa o Timer Counter 
	TCCR1B = 0x01;								// Trigger descendente
	TIFR1 = 1<<ICF1;							// Coloca a flag ICP a 0
	TIFR1 = 1<<TOV1;							// Coloca a flag do TimerOverflow a 0
	TimerOverflow = 0;							// Limpa a flag do TimerOverflow

	while ((TIFR1 & (1 << ICF1)) == 0);			// Espera pelo flanco descendente
	count = ICR1 + (65535 * TimerOverflow);											
	distance = count / 58;						// 8MHz Timer freq, sound speed =343 m/s
	flag_usom=0;
}	

void lerad(void){
	
	ADCSRA|=(1<<6);					// Início de conversao
	while((ADCSRA &(1<<ADSC))!=0);  // Avança quando a conversão acabar
		S_M=ADCH;
	
	if(S_M>155)
		flag_MEIO=1;				// Estes nao só aumentam a margem de erro do funcionamento dos sensores IV, como permite que o AGV funcione
	if (S_M<100)					// em superficies diferentes de preto e branco
		flag_MEIO=0;
	
	ADMUX=0b01100001;				// Liga o ADC1
	ADCSRA|=(1<<6);					// Início de conversao
	while((ADCSRA &(1<<ADSC))!=0);  // Avança quando a conversão acabar
		S_ESQ=ADCH;
	
	if(S_ESQ>155)
		flag_ESQ=1;
	if (S_ESQ<100)
		flag_ESQ=0;
	
	ADMUX=0b01100010;				// Liga o ADC2
	ADCSRA|=(1<<6);					// Início de conversao
	while((ADCSRA &(1<<ADSC))!=0);  // Avança quando a conversão acabar
		S_DIR=ADCH;
	
	if(S_DIR>155)
		flag_DIR=1;
	if (S_DIR<100)
		flag_DIR=0;
	
	ADMUX=0b01100000;				// Liga o ADC0	
}


void inversao(){	
	PORTD=0b00001010;
	flag_tudozero=1;
	OCR2B=250;
	OCR2A=250;
}

int main(){					//Função principal do programa
	
	init();					//Invoca a funçao inic	  // 1 - PRETO  0 - BRANCO
	while(1){
		
		if(flag_usom==1)
			usom();	
		
		lerad();			//FUNÇÃO QUE LÊ OS SENSORES IV
		
		/*TENDO LIDO OS SENSORES, VAI TOMAR AS DECISÕES QUE DEVE EM CONFORMIDADE DE MODO A SEGUIR A LINHA*/
		if (distance>10){
			PORTC|=0b00000000;
					
		if(flag_entroncamento==0){		
	
		if(flag_MEIO==1){
			
			flag_tudozero=0;
			
			PORTD=0b00001001;
			if (flag_ESQ==1 && flag_DIR==0){		// Vira para a esquerda
				estado=1;
				OCR2A=80;				
				OCR2B=200;		
			}
			
			if (flag_ESQ==0 && flag_DIR==0){		// Anda para a frente
				estado=2;
				OCR2A=200;				//165
				OCR2B=200;		
			}
			
			if (flag_ESQ==0 && flag_DIR==1){		// Vira para a direita
				estado=3;
				OCR2A=200;
				OCR2B=80;				//120
			}
			
			if (flag_ESQ==1 && flag_DIR==1){		// Entra numa nova verificação, ativando uma flag
				estado=4;
				OCR2A=120;							
				OCR2B=120;
				flag_entroncamento=1;				
			}
		}
		
		if (flag_tudozero==0){
		
		if (flag_MEIO==0){							//Correcoes
			
			if (flag_ESQ==1 && flag_DIR==0){		// Ajusta para a esquerda
				estado=5;
				OCR2A=35;		//35
				OCR2B=120;
			}
			
			if (flag_ESQ==0 && flag_DIR==0){		// Como teoricamente sai da pista, assume o estado lógico anterior
			
				switch(estado){
					case 1:
							OCR2A=50;
							OCR2B=230;
							break;
					case 2:
							OCR2A=150;
							OCR2B=150;
							break;
					case 3:
							OCR2A=230;
							OCR2B=50;
							break;
					case 4:
							OCR2A=120;
							OCR2B=120;
							break;
					case 5:
							OCR2A=35;
							OCR2B=120;
							break;
					case 6:
							OCR2A=120;
							OCR2B=35;
							break;
					case 7:
							OCR2A=0;
							OCR2B=0;
							break;
					default:
							 break;
				}
	//}
		}
			
			if (flag_ESQ==0 && flag_DIR==1){		// Ajusta para a direita
				estado=6;
				OCR2A=120;		
				OCR2B=35;		//35
			}
			
			if (flag_ESQ==1 && flag_DIR==1){		// De acordo com a montagem, é impossível acontecer
				estado=7;
				OCR2A=0;
				OCR2B=0;
			}
		}
	}
}

		
		if(flag_entroncamento==1){
				
				if (flag_MEIO==1 && flag_ESQ==1 && flag_DIR==1){		 // Move-se para a frente
					OCR2A=150;
					OCR2B=150;
				}
				
				if (flag_MEIO==0 && flag_ESQ==0 && flag_DIR==0){						
					inversao();				
					flag_entroncamento = 0;					
				}
				
				if (flag_MEIO==1 && flag_ESQ==0 && flag_DIR==0){		// Encontra a linha outra vez
					OCR2A=190;
					OCR2B=190; // Continua a andar normalmente 
					flag_entroncamento = 0;
				}		
			}	
		}
			
			else{
				PORTC|=0b00010000;
				OCR2A=120;
				OCR2B=120;
				_delay_us(10);
				OCR2A=80;
				OCR2B=80;
				_delay_us(10);
				OCR2A=50;
				OCR2B=50;
				_delay_us(10);
				OCR2A=0;
				OCR2B=0;				
			
			}
		}
	return 0;
}
