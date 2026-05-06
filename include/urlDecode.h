#pragma once

#include <Arduino.h>

String urlDecode(const String& encoded) {
    String decoded = "";
    char temp[] = "0x00"; // Temporary variable to hold the hex value

    for (size_t i = 0; i < encoded.length(); i++) {
        char c = encoded.charAt(i);
        if (c == '%') {
            if (i + 2 < encoded.length()) { // Ensure there are enough characters for a valid encoding
                temp[2] = encoded.charAt(i + 1);
                temp[3] = encoded.charAt(i + 2);
                decoded += (char)strtol(temp, nullptr, 16); // Convert hex to char
                i += 2; // Skip the next two characters as they are part of the encoding
            }
        } else if (c == '+') {
            decoded += ' '; // Convert '+' to space
        } else {
            decoded += c; // Regular character, add to decoded string
        }
    }
    return decoded;
}