#include "Configs.h"
#include <Arduino.h>

String Command::ToString()
{
  String result = (String)cmd;
  result += (String)val;
  return result;
}