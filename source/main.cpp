/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <events/mbed_events.h>
#include "mbed.h"
#include "BLE.h"
#include "ble/services/HealthThermometerService.h"
#include "pretty_printer.h"
#include "IKS/x_nucleo_iks01a1.h"

DigitalOut led1(LED1, 1);

const static char DEVICE_NAME[] = "Therm";

static events::EventQueue event_queue(/* event count */ 16 * EVENTS_EVENT_SIZE);

static X_NUCLEO_IKS01A1 *meb = X_NUCLEO_IKS01A1::Instance(D14, D15);

static TempSensor *temp_sensor1 = meb->ht_sensor;

class BatteryDemo : ble::Gap::EventHandler {
public:
    BatteryDemo(BLE &ble, events::EventQueue &event_queue) :
        _ble(ble),
        _event_queue(event_queue),
        _thermometer_uuid(GattService::UUID_HEALTH_THERMOMETER_SERVICE),
        _current_temperature(38),
        _thermometer_service(NULL),
        _adv_data_builder(_adv_buffer) { }

    void start() {
        _ble.gap().setEventHandler(this);

        _ble.init(this, &BatteryDemo::on_init_complete);

        _event_queue.call_every(5, this, &BatteryDemo::blink);
        _event_queue.call_every(10, this, &BatteryDemo::update_sensor_value);

        _event_queue.dispatch_forever();
    }

private:
    /** Callback triggered when the ble initialization process has finished */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *params) {
        if (params->error != BLE_ERROR_NONE) {
            print_error(params->error, "Ble initialization failed.");
            return;
        }

        print_mac_address();

        /* Setup primary service. */
        _thermometer_service = new HealthThermometerService(_ble, _current_temperature, HealthThermometerService::LOCATION_EAR);

        start_advertising();
    }

    void start_advertising() {
        /* Create advertising parameters and payload */

        ble::AdvertisingParameters adv_parameters(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(10))
        );

        _adv_data_builder.setFlags();
        _adv_data_builder.setLocalServiceList(mbed::make_Span(&_thermometer_uuid, 1));
        _adv_data_builder.setAppearance(ble::adv_data_appearance_t::THERMOMETER_EAR);
        _adv_data_builder.setName(DEVICE_NAME);

        /* Setup advertising */

        ble_error_t error = _ble.gap().setAdvertisingParameters(
            ble::LEGACY_ADVERTISING_HANDLE,
            adv_parameters
        );

        if (error) {
            print_error(error, "_ble.gap().setAdvertisingParameters() failed");
            return;
        }

        error = _ble.gap().setAdvertisingPayload(
            ble::LEGACY_ADVERTISING_HANDLE,
            _adv_data_builder.getAdvertisingData()
        );

        if (error) {
            print_error(error, "_ble.gap().setAdvertisingPayload() failed");
            return;
        }

        /* Start advertising */

        error = _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

        if (error) {
            print_error(error, "_ble.gap().startAdvertising() failed");
            return;
        }
    }

    void update_sensor_value() {
        float value1;
        temp_sensor1->get_temperature(&value1);
        if (_ble.gap().getState().connected) {
            _current_temperature = int(value1*100) % 100;
            _thermometer_service->updateTemperature(_current_temperature);
        }
    }

    void blink(void) {
        led1 = !led1;
    }

private:
    /* Event handler */

    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent&) {
        _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
    }

private:
    BLE &_ble;
    events::EventQueue &_event_queue;

    UUID _thermometer_uuid;

    uint8_t _current_temperature;
    HealthThermometerService *_thermometer_service;

    uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;
};

/** Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

int main()
{
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);

    BatteryDemo demo(ble, event_queue);
    demo.start();

    return 0;
}
