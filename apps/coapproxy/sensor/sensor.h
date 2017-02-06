/*
 * read temperature and humidity sensor from DHT11 and DHT22
 * http://www.uugear.com/portfolio/read-dht1122-temperature-humidity-sensor-from-raspberry-pi/
 */


#ifndef SENSOR_H
#define SENSOR_H


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wiringPi.h>
/*
 * read 40bits (5byte) and verify checksum in last byte
 * 
 */
int dht11_dat[5] = { 0, 0, 0, 0, 0 };

/*!
 * @param data temperature and humidity value from DHT11 or DHT22 sensor
 *
 */
void read_dht11_dat(char* data);


#endif
