// This is display helper

  // initialize display SSD1309
void intDisp(){
  display.clearDisplay();
  display.setTextSize(TextSize);
  display.setTextColor(SSD1306_WHITE);
  delay(500);
  Serial.println("[V] Display initize complete");
}

/* print text on I2C screen
Input - text, operation
Operation = 0 system message
Operation = 1 outgoing message
Operation = 2 incomming message*/
void printSCR(String PrintText, int Operation){
  String TempNewLine;
  if (Operation == 0){
    TempNewLine = "* " + PrintText; //system  message
  } else if(Operation == 1) {
    TempNewLine = "->" + PrintText; //outgoming message
  } else if(Operation == 2) {
    TempNewLine = "<-" + PrintText; //incoming message
  } else {
    Serial.println("[X] error Operation value");
  }
  int charsPerLine = SCREEN_WIDTH / 6; // 6px per char at TextSize 1
  // Split text into chunks that fit on screen width
  int numChunks = (TempNewLine.length() + charsPerLine - 1) / charsPerLine;
  // Shift existing lines down by numChunks in the buffer
  int maxShift = (totalLines < LINE_BUFFER) ? totalLines : LINE_BUFFER - 1;
  for (int i = maxShift; i >= numChunks; i--) {
    if (i < LINE_BUFFER) lines[i] = lines[i - numChunks];
  }
  // Fill top lines with chunks (first chunk at index 0, continuation below)
  for (int c = 0; c < numChunks && c < LINE_BUFFER; c++) {
    int startIdx = c * charsPerLine;
    lines[c] = TempNewLine.substring(startIdx, startIdx + charsPerLine);
  }
  totalLines += numChunks;
  if (totalLines > LINE_BUFFER) totalLines = LINE_BUFFER;
  scrollOffset = 0; // Reset scroll to newest on new message
  redrawDisplay();
}

  // draw display again using scrollOffset
void redrawDisplay() {
  display.clearDisplay();
  for (int i = 0; i < MAX_LINES; i++) {
    int bufIdx = scrollOffset + i;
    if (bufIdx < LINE_BUFFER) {
      display.setCursor(0, i * LINE_HEIGHT);
      display.print(lines[bufIdx]);
    }
  }
  LineDraw();
}

// Scroll up - show older messages
void scrollUp() {
  int maxOffset = totalLines - MAX_LINES;
  if (maxOffset < 0) maxOffset = 0;
  if (scrollOffset < maxOffset) {
    scrollOffset++;
    redrawDisplay();
  }
}

// Scroll down - show newer messages
void scrollDown() {
  if (scrollOffset > 0) {
    scrollOffset--;
    redrawDisplay();
  }
}

  // Draw input line and separator
void LineDraw(){
  display.setCursor(0, SCREEN_HEIGHT - LINE_HEIGHT);
  int BottomLineY = SCREEN_HEIGHT - LINE_HEIGHT - 2;
  display.drawFastHLine(0, BottomLineY, SCREEN_WIDTH, WHITE);
  display.display();
}

// display logo
void LogoDisplay(){
  Serial.println("[*] Logo display on screen");
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(17, 20);
  display.println(F("3NCRYP2P"));
  display.setTextSize(1);
  display.println();
  display.print(F("  v "));
  display.println(SoftwareVer);
  display.display();
  delay(3500); //delay
  display.setTextSize(TextSize);
}