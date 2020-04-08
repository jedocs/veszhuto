#ifndef SerialSniffer_h
#define SerialSniffer_h

class SerialSniffer
  : public Stream

{
  public:
    SerialSniffer(Stream& data, Stream& dump)
      : _data(data), _dump(dump)

    {}
  private:
    uint8_t state = 0;

    virtual size_t write(uint8_t ch) {
      return _data.write(ch);
    }
    virtual int read() {
      int ch = _data.read();

      switch (state)
      {
        case 0:
          if (ch == '+') {
            _dump.write('0');
            state = 1;
          }
          else {
            state = 0;
          }
          break;

        case 1:
          if (ch == 'C') {
            _dump.write('1');
            state = 2;
          }
          else {
            state = 0;
          }
          break;

        case 2:
          if (ch == 'M') {
            _dump.write('2');
            state = 3;
          }
          else {
            state = 0;
          }
          break;


        case 3:
          if (ch == 'T') {
            _dump.write('3');
            state = 4;
          }
          else {
            state = 0;
          }
          break;


        case 4:
          if (ch == ':') {
            _dump.write('4');
            state = 5;
          }
          else {
            state = 0;
          }
          break;


        case 5:
          if (ch == ' ') {
            _dump.write('5');
            state = 6;
          }
          else {
            state = 0;
          }
          break;

        case 6:
          if (ch == '"') {
            _dump.write('6');
            state = 7;
          }
          else {
            state = 0;
          }
          break;

        case 7:
          if (ch == '+') {
            _dump.println("SMS***");
            state = 0;
          }
          else {
            state = 0;
          }
          break;

        default:
          state = 0;

      }

      //_dump.write(ch);

      return ch;
    }
    virtual int available() {
      return _data.available();
    }
    virtual int peek()      {
      return _data.peek();
    }
    virtual void flush()    {
      _data.flush();
    }


  private:
    Stream& _data;
    Stream& _dump;

};

#endif
