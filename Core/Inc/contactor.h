#pragma once

#include "timers.h"

class Contactor {

	ADC_& adc;
	Service<In_data, Out_data>& service;
	Timer timer;
	bool on{false};
	bool on_off{false};
	bool enable{false};

public:

	Contactor(ADC_& adc, Service<In_data, Out_data>& service) : adc{adc}, service{service} {}

	void start(){
		on_off = true;
		enable = false;
		timer.stop();
	}

	void stop(){
		TIM4->CCR1 = 0;
		HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1);
		enable = false;
		on_off = false;
		timer.stop();
	}

	bool is_on() {
		return on;
	}

	void operator()(){
		if(on_off and not enable /*and service.outData.high_voltage >= 300*/) {
			HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
			TIM4->CCR1 = 1799;
			if(is_on() and not timer.isCount()) {
				timer.start(1000);
			}

			if(timer.done()){
				timer.stop();
				enable = true;
			}
		}

		if(enable){
			if(service.outData.voltage_board > 240) TIM4->CCR1 = 900;
			else if (service.outData.voltage_board < 200) TIM4->CCR1 = 1799;
			else
				TIM4->CCR1 = 5220 - 18 * service.outData.voltage_board;
		}

		on = HAL_GPIO_ReadPin(GPIOD, Contactor_Pin);
	}



};
