#include <Arduino.h>

#include "utils/logger/logger.h"

Logger logger( "main" );

void setup() {
    logger.begin();

    logger.log( LogLevel::INFO, "Hello World" );
}

void loop() {
    logger.log( LogLevel::INFO, "Hello world in da loop" );
    delay( 5000 );
} 