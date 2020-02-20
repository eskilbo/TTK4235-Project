#include <stdio.h>
#include <stdlib.h>
#include "hardware.h"
#include "elevator.h"
#include "timer.h"

int FLOOR_COUNT = 4;
int BUTTON_COUNT = 3;

// add ints for up, down and upanddown

static int current_floor = 0;
static int current_direction = HARDWARE_MOVEMENT_STOP;
// add FLOOR_COUNT
int elevator_queue[4];

CurrentState state = IDLE;

int elevator_get_current_floor(){
    for(int i = 0; i < FLOOR_COUNT; i++){
        if(hardware_read_floor_sensor(i)){
            return i+1;
        }
    }
    return 0;
}

void elevator_set_current_floor(){
    if(elevator_get_current_floor()){
        current_floor = elevator_get_current_floor()-1;
    }
}

void print_elevator_queue(){
    for(int i = 0; i < FLOOR_COUNT; ++i){
        printf("Floor %d, has index %d \n", i+1, elevator_queue[i]);
    }
}

void order_buttons_up() {
    for (int i = 0; i < BUTTON_COUNT; ++i){
        if (hardware_read_order(i, HARDWARE_ORDER_UP)) {
            hardware_command_order_light(i, HARDWARE_ORDER_UP, 1);
            if(elevator_queue[i] == BOTH_OR_INSIDE || elevator_queue[i] == DOWN){
                elevator_queue[i] = BOTH_OR_INSIDE;
            } else{
                elevator_queue[i] = UP;
            }
        }
    }
}

void clear_order_buttons_up() {
    for (int i = 0; i < BUTTON_COUNT; ++i){
        if(elevator_queue[i]== 0 || elevator_queue[i]== 2){
            hardware_command_order_light(i, HARDWARE_ORDER_UP, 0);
        }
    }
}

void order_buttons_down() {
    for (int i = 1; i < BUTTON_COUNT+1; ++i){
        if (hardware_read_order(i, HARDWARE_ORDER_DOWN)) {
            hardware_command_order_light(i, HARDWARE_ORDER_DOWN, 1);
            if(elevator_queue[i] == BOTH_OR_INSIDE || elevator_queue[i] == UP){
                elevator_queue[i] = BOTH_OR_INSIDE;
            } else{
                elevator_queue[i] = DOWN;
            }
        }
    }
}

void clear_order_buttons_down() {
    for (int i = 1; i < BUTTON_COUNT + 1; ++i){
        if(elevator_queue[i] == 0 || elevator_queue[i]== 1){
            hardware_command_order_light(i, HARDWARE_ORDER_DOWN, 0);
        }
    }
}

void order_buttons_inside(){
    for(int i = 0; i < BUTTON_COUNT + 1; ++i){
        if (hardware_read_order(i, HARDWARE_ORDER_INSIDE)) {
            hardware_command_order_light(i, HARDWARE_ORDER_INSIDE, 1);
            elevator_queue[i] = BOTH_OR_INSIDE;
            //printf("Inside button on floor %d pressed ", i);
        }
    }
}


void clear_order_buttons_inside() {
    for (int i = 0; i < BUTTON_COUNT + 1; ++i){
        if(elevator_queue[i] != 3){
            hardware_command_order_light(i, HARDWARE_ORDER_INSIDE, 0);
        }
    }
}

void stop_button(){
    if(hardware_read_stop_signal()){
        hardware_command_stop_light(1);
    }
}

void floor_lights(){
    //printf("Current floor: %d ", current_floor);
    hardware_command_floor_indicator_on(current_floor);
}

// endre til handle_buttons()
void buttons(){
    order_buttons_up();
    clear_order_buttons_up();
    order_buttons_down();
    clear_order_buttons_down();
    order_buttons_inside();
    clear_order_buttons_inside();
    stop_button();
    floor_lights();
}


void elevator_startup(){
     // Initalize hardware
    int error = hardware_init();
    if(error != 0){
        fprintf(stderr, "Unable to initialize hardware\n");
        exit(1);
    }

    // Move to first floor
    while(elevator_get_current_floor() != 1){
        hardware_command_movement(HARDWARE_MOVEMENT_DOWN);
    }
    hardware_command_movement(HARDWARE_MOVEMENT_STOP);

}


void travel_to_destination(int destination_floor, HardwareMovement direction){
    while(destination_floor != current_floor){
        buttons();
        elevator_set_current_floor();
        // new order when already travelling
        hardware_command_movement(direction);
    }
    hardware_command_movement(HARDWARE_MOVEMENT_STOP);
}

void run_elevator(){
    printf("Current floor: %d ", current_floor);
    buttons();
    elevator_startup();

    while(1){
        buttons();
        elevator_set_current_floor();
        //printf("UP: %d ", UP);
        switch(state){
            case IDLE:
                hardware_command_movement(HARDWARE_MOVEMENT_STOP);
                hardware_command_door_open(0);
                for(int i = 0; i < FLOOR_COUNT; ++i){
                    if(elevator_queue[i]){
                        if(elevator_get_current_floor() == i+1){
                            state = DOOR;
                        } else{
                            state = RUNNING;
                        } 
                    }
                }
                break;
                
            case RUNNING:

                // ordre på samme etg
                // stopper ikke helt nederst
                // stopper i 2* 3sek hvis man trykker på samme etg

                for(int i = 0; i < FLOOR_COUNT; ++i){
                    if(elevator_queue[i]){
                        if((elevator_queue[i] == 1 || elevator_queue[i] == 3) && i > current_floor && current_direction != HARDWARE_MOVEMENT_DOWN){
                            travel_to_destination(i, HARDWARE_MOVEMENT_UP);
                            state = DOOR;
                        } else if((elevator_queue[i] == 2 || elevator_queue[i] == 3) && i < current_floor && current_direction != HARDWARE_MOVEMENT_UP){
                            travel_to_destination(i, HARDWARE_MOVEMENT_DOWN);
                            state = DOOR;
                        }
                    }
                }
                break;

            case DOOR:
                hardware_command_door_open(1);
                timer_start_timer(3000);
                elevator_queue[current_floor] = 0;
                buttons();
                while(!timer_check_expired()){
                    buttons();
                }
                if(hardware_read_obstruction_signal()){
                    state = OBSTRUCT;
                } else{
                    hardware_command_door_open(0);
                    for(int i = 0; i < FLOOR_COUNT; ++i){
                        if(elevator_queue[i]){
                            if(elevator_get_current_floor() == i+1){
                                state = DOOR;
                            } else{
                                state = RUNNING;
                            } 
                        } else{
                            state = IDLE;
                        }
                    }
                }
                
                break;

            case EMERGENCY_STOP:
                hardware_command_movement(HARDWARE_MOVEMENT_STOP);
                if(elevator_get_current_floor()){
                    while(hardware_read_stop_signal()){
                        hardware_command_door_open(1);
                    }
                    timer_start_timer(3000);
                    while(!timer_check_expired()){
                        buttons();
                    }
                    hardware_command_door_open(0);   
                }
                state = IDLE;
                break;

            case OBSTRUCT:
                // orders_update_order_array();
                if (elevator_get_current_floor()){
                        timer_start_timer(3000);
                        while(!timer_check_expired()){
                        }
                        state = DOOR;
                    }else{
                        state = IDLE;
                    }
                break;

        }
        
    }
}