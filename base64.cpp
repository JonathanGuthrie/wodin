/*
 * Copyright 2013 Jonathan R. Guthrie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base64.hpp"

std::string base64_encode(const std::string &in) {
  const std::string map = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  int len = (int)in.size();
  unsigned long value;

  for(int i=0; i<len;) {
    switch(len-i) {
    case 1:
      // There's only one character left, so I have to pad with two '='
      value = 0xff & in[i];
      result += map[0x3f & (value >> 2)];
      result += map[0x3f & (value << 4)];
      result += "==";
      i = len;
      break;

    case 2:;
      // There's only one character left, so I have to pad with one '='
      value = ((0xff & in[i]) << 8) | (0xff & in[i+1]);
      result += map[0x3f & (value >> 10)];
      result += map[0x3f & (value >> 4)];
      result += map[0x3f & (value << 2)];
      result += "=";
      i = len;
      break;

    default:
      // I encode three characters in four
      value = ((0xff & in[i]) << 16) | ((0xff & in[i+1]) << 8) | (0xff & in[i+2]);
      result += map[0x3f & (value >> 18)];
      result += map[0x3f & (value >> 12)];
      result += map[0x3f & (value >> 6)];
      result += map[0x3f & (value >> 0)];
      i += 3;
      break;
    }
  }

  return result;
}


static short base64_char_decode_map(const char c) {
  short result;

  if (isupper(c)) {
    result = c - 'A';
  }
  else  {
    if (islower(c)) {
      result = 26 + c - 'a';
    }
    else {
      if (isdigit(c)) {
	result = 52 + c - '0';
      }
      else {
	if ('+' == c) {
	  result = 62;
	}
	else {
	  if ('/' == c) {
	    result = 63;
	  }
	  else {
	    result = -1;
	  }
	}
      }
    }
  }
  return result;
}


std::string base64_decode(const std::string &s) {
  std::string result;
  unsigned long value;
  short m;
  int len, not_there, accumulated;
  len = s.size();
  not_there = 0;
  accumulated = 0;

  for (int i=0; i<len; ++i) {
    if ('=' != s[i]) {
      m= base64_char_decode_map(s[i]);
      if (0 <= m) {
	++accumulated;
	value = (value << 6) | m;
      }
    }
    else {
      m = 0;
      ++accumulated;
      ++not_there;
      value = (value << 6) | m;
    }

    if (4 == accumulated) {
      switch(not_there) {
      case 1:
	result += (unsigned char)(0xff & (value>>16));
	result += (unsigned char)(0xff & (value>>8));
	break;

      case 2:
	result += (unsigned char)(0xff & (value>>16));
	break;

      default:
	result += (unsigned char)(0xff & (value>>16));
	result += (unsigned char)(0xff & (value>>8));
	result += (unsigned char)(0xff & (value>>0));
	break;
      }
      accumulated = 0;
      not_there = 0;
    }
  }
  return result;
}


#ifdef DEBUG_BASE64
#include <iostream>

int
main (int argc, char *argv[]) {
  std::string s;
  s += '\0';
  s += "jguthrie";
  s += '\0';
  s += "test";
  CStdString e = base64_encode(s);
  std::cout << "Encoding the string gets \"" << e << "\"\n";

  std::cout << "Decoding \"" << e << "\"\n";
  s = base64_decode(e);
  for (unsigned i=0; i<s.size(); ++i) {
    printf("Character s[%d] = '%c' (0x%02x)\n", i, isprint(s[i]) ? s[i] : '.',
	   s[i]);
  }
}
#endif

#ifdef UTILITY
#include <iostream>


int
main(int argc, char **argv) {
  if (argc > 1) {
    std::string s(argv[1]);
    std::string e = base64_encode(s);
    std::cout << "Encoding the string \"" << s << "\" gives \"" << e << "\"\n";
    e = base64_decode(s);
    std::cout << "Decoding the string \"" << s << "\" gives \"" << e << "\"\n";
  }
  return 0;
}
#endif /* utility */
