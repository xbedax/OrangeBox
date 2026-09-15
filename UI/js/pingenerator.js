/*
 * PIN generator
 *
 * Pico.css - https://picocss.com
 * Copyright 2019-2023 - Licensed under MIT
 */

// Config

const maxPin = 999999;
let maxPinLength = maxPin.toString().length;
let newPin = "";

function getRandomInt(min, max) {
    min = Math.ceil(min);
    max = Math.floor(max);
    newValue =  Math.floor(Math.random() * (max - min + 1)) + min;
    return newValue;
}

function getPin(event) {
  event.preventDefault();
  newPin = getRandomInt(0, maxPin).toString();
  console.log('Generated:')
  console.log(newPin);
  pinLength = newPin.length;
  for (i = 0; i < (maxPinLength - pinLength); i++) {
    newPin = "0" + newPin;
  }
  console.log('Proposed:');
  console.log(newPin);
  document.getElementById("pinvalue").value = newPin; 
}

