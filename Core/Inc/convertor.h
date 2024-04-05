#pragma once

#include "adc.h"
#include "service.h"
#include "contactor.h"
#include "interrupt.h"
#include "pin.h"

class Convertor {

	enum {SYNCHRON = false, ASYNCHRON = true};
	enum State {wait, starting} state{wait};

	ADC_& adc;
	Service<In_data, Out_data>& service;
	Contactor& contactor;
	Interrupt& period_callback;
	Interrupt& adc_comparator_callback;
	Pin& led_red;
	Pin& led_green;
	Pin& ventilator;
	Pin& unload;
	Pin& condens;
	Pin& TD_DM;
	Pin& SP;
	Pin& Start;
	Pin& Motor;

	Timer timer;
	Timer rerun;
	Timer timer_stop;
	Timer restart;
	Timer test;

	uint16_t sin_table[qty_point]{3600, 5514, 6764
								, 7199, 6764, 6764
								, 7199, 6764, 5514
								, 3600, 1250,    0
								,    0,    0,    0
								,    0,    0, 1250
								};
	uint8_t r{0};
	uint8_t k{0};
	uint8_t m{6};
	uint8_t n{12};
	uint32_t Km{5};
	uint16_t Kp{1150};
	uint16_t frequency{100};
	uint16_t max_current{25};
	uint16_t need_current{4000};
	int16_t e{0};
	uint16_t time{2};
	uint16_t U_phase{0};
	bool U_stop{false};
	bool motor{false};
	uint16_t U_phase_max{0};
	uint8_t offset{25};
	uint8_t error{0};
	uint8_t error_S{0};
	uint8_t error_A{0};
	uint8_t error_C{0};
	uint8_t error_F{0};
	uint16_t min_ARR{356};

	bool enable{true};
	bool phase{false};
	bool cool{false};
	bool cold{false};

//	float radian = 10 * 3.14 / 180;
	uint32_t div_f = 1'800'000 / (qty_point);

	using Parent = Convertor;

	struct TIM3_interrupt: Interrupting {
		Parent &parent;
		TIM3_interrupt(Parent &parent) :
				parent(parent) {
			parent.period_callback.subscribe(this);
		}
		void interrupt() override {
			parent.period_interrupt();
		}
	} tim3_interrupt { *this };

	struct adc_comparator_interrupt: Interrupting {
		Parent &parent;
		adc_comparator_interrupt(Parent &parent) :
				parent(parent) {
			parent.adc_comparator_callback.subscribe(this);
		}
		void interrupt() override {
			parent.comparator_interrupt();
		}
	} adc_comparator_ { *this };

	void period_interrupt(){

		TIM1->CCR1 = Km * sin_table[k++] / 1000;
//		TIM1->CCR2 = Km * sin_table[m++] / 1000;
		TIM1->CCR3 = Km * sin_table[n++] / 1000;

		if (k >= qty_point) {k = 0;}
		if (m >= qty_point) {m = 0;}
		if (n >= qty_point) {n = 0;}

//		TIM3->ARR = (div_f / (frequency)) * 10 - 1;

		HAL_ADCEx_InjectedStart_IT(&hadc2);

	}

	void comparator_interrupt() {

	}

public:

	Convertor(ADC_& adc, Service<In_data, Out_data>& service, Contactor& contactor, Interrupt& period_callback, Interrupt& adc_comparator_callback
			, Pin& led_red, Pin& led_green, Pin& ventilator, Pin& unload, Pin& condens, Pin& TD_DM, Pin& SP, Pin& Start, Pin& Motor)
	: adc{adc}, service{service}, contactor{contactor}, period_callback{period_callback}, adc_comparator_callback{adc_comparator_callback}
	, led_red{led_red}, led_green{led_green}, ventilator{ventilator}, unload{unload}, condens{condens}, TD_DM{TD_DM}, SP{SP}, Start{Start}, Motor{Motor}
	{rerun.time_set = 0; timer_stop.time_set = 0; restart.time_set = 0; /*stop();*/ motor = Motor;}

	void operator() (){

		service();
		contactor();

		service.outData.PWM = Km;
		service.outData.error.on = Start;
		service.outData.U_phase = U_phase;
		service.outData.error.overheat_c = not bool(TD_DM);
		service.outData.error.HV_low = /*(service.outData.high_voltage <= 300) or*/ U_stop;
		service.outData.error.voltage_board_low = (service.outData.voltage_board <= 18);
//		service.outData.error.voltage_board_low = false;
//		service.outData.error.HV = adc.is_error_HV();//service.outData.high_voltage >= 850;
		service.outData.max_current_A = min_ARR;
		service.outData.max_current_C = U_phase_max;

		service.outData.voltage_board = Kp;
		service.outData.max_current = TIM3->ARR;

		if(service.outData.high_voltage <= 300) U_stop = true;
		else if(service.outData.high_voltage > 300) {U_stop = false; adc.reset_error_HV();}

		if (service.outData.error.overheat_fc |= service.outData.convertor_temp >= 60) {
			service.outData.error.overheat_fc = service.outData.convertor_temp >= 50;
		}

		if (cool |= service.outData.convertor_temp >= 40) {
			cool = service.outData.convertor_temp >= 30;
		}

		if(enable)
			ventilator = cool;
		else
			ventilator = false;

		if(contactor.is_on() and enable) alarm();

		switch(state) {
		case wait:
			motor = Motor;

if(motor == ASYNCHRON) {

	/*
	adc.set_max_current(35);
	adc.set_max_current_phase(36);
	if (service.outData.high_voltage > 300 and service.outData.high_voltage < 540) {
		U_phase_max = ((((service.outData.high_voltage / 20) * 990) / 141) * 115) / 100;
		min_ARR = (div_f / ((U_phase_max) * 9)) * 22; // 5/22 = 50/220
	} else {
		U_phase_max = 220;
		min_ARR = 1100;
	}

*/

	adc.set_max_current(20);
	adc.set_max_current_phase(24);
	unload = false;
	if (service.outData.high_voltage > 300 and service.outData.high_voltage < 540) {
		U_phase_max = ((((service.outData.high_voltage / 20) * 990) / 141) * 115) / 100;
		min_ARR = (div_f / ((U_phase_max) * 5)) * 22; // 5/22 = 50/220
		if(min_ARR <= 2081) min_ARR = 2081;
	} else {
		U_phase_max = 220;
		min_ARR = 2080;
	}


} else if (motor == SYNCHRON) {

	adc.set_max_current(18);
	adc.set_max_current_phase(28);
			unload = true;
			if(service.outData.high_voltage > 300 and service.outData.high_voltage < 540) {
				U_phase_max = ((((service.outData.high_voltage / 20) * 940) / 141) * 115) / 100;
				min_ARR = ( (div_f / (U_phase_max)) * 50) / 70; // 70/53 = 280/212
				if(min_ARR < 362) min_ARR = 362;
			} else {
				U_phase_max = 212;
				min_ARR = 362;
			}

}

			enable = Start and not rerun.isCount() /*and not service.pressure_is_normal()*/
					 and not service.outData.error.overheat_fc and not service.outData.error.overheat_c
					 /*and not service.outData.error.HV */and not service.outData.error.HV_low
					 and not service.outData.error.voltage_board_low and (error < 3) and not U_stop /* and not service.outData.error.contactor*/;

			if(rerun.done()) rerun.stop();

//			if(test.done()) test.stop();

			if(error >= 3 and not restart.isCount()) {
				restart.start(5'000);
			}

			if(error >= 3 and restart.done()) {
				restart.stop();
				error = 0;
			}

			if (enable){
				rerun.stop();
				contactor.start();
				if(contactor.is_on()) {
					pusk();
					state = State::starting;
				}
			}
//			else stop();

			if (not Start) {
//				U_stop = false;
				rerun.stop();
				rerun.time_set = 0;
				restart.stop();
				restart.time_set = 0;
				error = 0;
				led_red = false;
				adc.reset_error();
				phase = false;
			}

			break;
		case starting:

			adc.what_Km(Km);

if(motor == ASYNCHRON) {

/////////////////CONDITIONER\\\\\\\\\\\\\\\

	/*
	if (service.outData.high_voltage > 400 and service.outData.high_voltage < 540) {
		U_phase_max = ((((service.outData.high_voltage / 20) * 990) / 141) * 115) / 100;
		min_ARR = (div_f / ((U_phase_max) * 9)) * 22; // 5/22 = 50/220
	} else {
		U_phase_max = 220;
		min_ARR = 1100;
	}

	U_phase = ((((service.outData.high_voltage / 20) * Km) / 141) * 112) / 100; // 31 = 620 / 20; 141 = sqrt(2) * 100; 115 = добавочный
	Km = offset + ((Kp * (div_f / TIM3->ARR) / (service.outData.high_voltage)));

	if (TIM3->ARR <= (min_ARR + 10)) {
		error = 0;
	}

	if (Kp > 12000) {
		Kp = 12000;
	}

	if (TIM3->ARR <= min_ARR) {
		if (U_phase - U_phase_max > 10) {
			Kp--;
		} else {
			if((U_phase_max - U_phase > 10))
				Kp++;
		}

		if (adc.current() > 200) {
			if (Kp > 5000) {
				Kp -= 4;
			}
		}
	}

	if (adc.current() < 35) {
		if (Kp < 12000) {
			Kp++;
		}
	}

	if (TIM3->ARR > (min_ARR + 5)) {
		if (adc.current() > 75) {
			if (Kp >= 6000) {
				Kp--;
			}
		}
	}
*/
/////////////////CONDITIONER\\\\\\\\\\\\\


	if (service.outData.high_voltage > 300 and service.outData.high_voltage < 540) {
		U_phase_max = ((((service.outData.high_voltage / 20) * 990) / 141) * 115) / 100;
		min_ARR = (div_f / ((U_phase_max) * 5)) * 22; // 5/22 = 50/220
		if(min_ARR <= 2081) min_ARR = 2081;
	} else {
		U_phase_max = 220;
		min_ARR = 2080;
	}

	U_phase = ((((service.outData.high_voltage / 20) * Km) / 141) * 112) / 100; // 31 = 620 / 20; 141 = sqrt(2) * 100; 115 = добавочный
	Km = offset + ( (Kp * (div_f / TIM3->ARR) / service.outData.high_voltage ) * 4 )/ 3;

	if (TIM3->ARR <= (min_ARR + 5)) {
		unload = true;
		error = 0;
	}

	if (Kp > 12000) {
		Kp = 12000;
	}

	if (TIM3->ARR <= min_ARR) {
		if (U_phase - U_phase_max > 10) {
			Kp--;
		} else {
			if(adc.current() < 120 and (U_phase_max - U_phase > 10))
			Kp++;
		}

		if (adc.current() > 160) {
			if (Kp > 5000) {
				Kp -= 4;
			}
		}
	}

	if (adc.current() < 35) {
		if (Kp < 12000) {
			Kp++;
		}
	}

	if (TIM3->ARR > (min_ARR + 5)) {
		if (adc.current() > 75) {
			if (Kp >= 6000) {
				Kp--;
			}
		}
	}


} else if(motor == SYNCHRON) {

//	service.outData.high_voltage = 550;

				if (service.outData.high_voltage > 300 and service.outData.high_voltage < 540 and not cold) {
					U_phase_max = ((((service.outData.high_voltage / 20) * 940) / 141) * 115) / 100;
					min_ARR = ((div_f / (U_phase_max)) * 50) / 70; // 70/53 = 280/212
					if(min_ARR < 362) min_ARR = 362;
				} else if (not cold){
					min_ARR = 362;
					U_phase_max = 212;
				}

				U_phase = ((((service.outData.high_voltage / 20) * Km) / 141) * 115) / 100; // 31 = 620 / 20; 141 = sqrt(2) * 100; 115 = добавочный
				U_phase += (U_phase_max - U_phase) * 10 / 50;
				Km = offset + Kp * (div_f / TIM3->ARR) / (service.outData.high_voltage);

				if (TIM3->ARR <= (min_ARR + 5)) {
					unload = false;
					error = 0;
				}


				if(TIM3->ARR <= min_ARR) {
					if ((U_phase > U_phase_max)) {
						if((U_phase - U_phase_max) > 10)
							Kp--;
					} else {
						if ((U_phase_max - U_phase > 10))
							Kp++;
					}

					if (adc.current() > 160) {
						if (Kp > 1250) {
							Kp -= 4;
						}
					}
				}
				if (adc.current() < 35) {
					if (Kp < 2200) {
						Kp++;
					}
				}
				if (TIM3->ARR > (min_ARR + 5)) {
					if (adc.current() > 110) {
						if(Kp > 1250) {
							Kp--;
						}
					}
				}

				if (Kp >= 2200) {
					Kp = 2200;
				}

} //else if(motor == SYNCHRON) {

			if (Km >= 990) {
				Km = 990;
			}

			if (timer.done()) {
				timer.stop();
				timer.start(time);

if(motor == ASYNCHRON) {

	if (TIM3->ARR != min_ARR) {
		if (TIM3->ARR > 6000) {
			TIM3->ARR -= 25;
		} else if (TIM3->ARR > min_ARR) {
			TIM3->ARR -= 5;
		} else {
			TIM3->ARR++;
		}
	}

} else if(motor == SYNCHRON) {
			if(TIM3->ARR != min_ARR) {
				if(TIM3->ARR > min_ARR) {
					if(TIM3->ARR > 624) {
						if(TIM3->ARR > 1500) {
							TIM3->ARR -= 32;
						} else {

							TIM3->ARR -= 3;
						}
					} else {
						TIM3->ARR-=1;
					}
				} else {
					TIM3->ARR++;
				}

				if(TIM3->ARR > 624) {
					time = 3;
				} else if (TIM3->ARR >= 554) {
					time = 6;
				} else if (TIM3->ARR < 554) {
					time = 8;
				}

			}
	}
} // else if(motor == SYNCHRON) {
			break;
		}
	}

	void pusk() {

if(motor == ASYNCHRON) {
		frequency = 60;
		Kp = 6000;
		time = 3;
		offset = 35;

} else if(motor == SYNCHRON) {
		frequency = 10;
		Kp = 1140;
		time = 2;
		offset = 38;
} // else if(motor == SYNCHRON) {

		Km = 5;
		TIM3->ARR = (div_f / (frequency)) * 10 - 1;

		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
		HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
		HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
		HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

		HAL_TIM_Base_Start_IT(&htim3);

		timer.start(time);
		adc.measure_value();

		service.outData.error.current_S = false;
		service.outData.error.current_A = false;
		service.outData.error.current_C = false;
		service.outData.error.phase_break = false;
		service.outData.error.HV = false;

		led_red = false;
	}

	void stop() {

		TIM1->CCR1 = 0;
		TIM1->CCR2 = 0;
		TIM1->CCR3 = 0;
		HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
		HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
		HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
		HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);
		HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
		HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_3);

		HAL_TIM_Base_Stop_IT(&htim3);
		timer.stop();
		contactor.stop();

		k = 0;
		m = 6;
		n = 12;
		state = State::wait;
		adc.measure_offset();

	}

	void alarm() {
		if((not Start or timer_stop.done()) or not contactor.is_on() /*or service.pressure_is_normal()*/
				     or service.outData.error.overheat_fc or service.outData.error.overheat_c
				     or service.outData.error.HV_low /*or service.outData.error.HV*/ or service.outData.error.voltage_board_low
					 )
		{
			if(not Start and not timer_stop.isCount()) {
				timer_stop.start(1000);
			}

			if(timer_stop.done() and not Start) {
				stop();
				timer_stop.stop();
			}

			if(not contactor.is_on() /*or service.pressure_is_normal()*/
				     or service.outData.error.overheat_fc or service.outData.error.overheat_c
				     or service.outData.error.HV_low /*or service.outData.error.HV*/ or service.outData.error.voltage_board_low) {
				stop();
				timer_stop.stop();
//				rerun.start(5000);
			}

		}
//if(motor == SYNCHRON) {
//
//	if (TIM3->ARR >= (min_ARR + 5)) {
//		if (adc.is_error()) {
//			adc.reset_error();
//			if(error_F++ >= 2) {
//				phase = true;
//				error_F = 0;
//				error++;
//			}
//			led_red = true;
//			stop();
//			service.outData.error.phase_break = true;
//			rerun.start(5000);
//		}
//	}
//
//}

		if(adc.is_error_HV()) {
			adc.reset_error_HV();
			error++;
			led_red = true;
			stop();
			service.outData.error.HV = true;
			rerun.start(5000);
		}

		if(adc.is_over_s() and not service.outData.error.current_S) {
			adc.reset_over_s();
			error++;
			led_red = true;
			stop();
			service.outData.error.current_S = true;
			rerun.start(5000);
		}

		if(adc.is_over_a() and not service.outData.error.current_A) {
			adc.reset_over_a();
			error++;
			led_red = true;
			stop();
			service.outData.error.current_A = true;
			rerun.start(5000);
		}

		if(adc.is_over_c() and not service.outData.error.current_C) {
			adc.reset_over_c();
			error++;
			led_red = true;
			stop();
			service.outData.error.current_C = true;
			rerun.start(5000);
		}

		adc.reset_measure();
	}

};

Interrupt period_callback;
Interrupt adc_comparator_callback;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef * htim){
	if(htim->Instance == TIM3) //check if the interrupt comes from ACD2
	{
		period_callback.interrupt();
	}
}

void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef* hadc){
	if(hadc->Instance == ADC2) //check if the interrupt comes from ACD2
	{
		adc_comparator_callback.interrupt();
	}
}

