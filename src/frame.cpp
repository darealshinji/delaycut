#include "frame.h"

Frame::Frame(QObject *parent) :
    QObject(parent), nBit(0)
{
}

const uchar* Frame::getData() const {
    return data;
}

void Frame::setData(const uchar* newData, const uint size) {
    nBit = 0;
    const uchar* src = newData;
    uchar* dst = data;
    for (uint count=0; count < size && count < MAXFRAMESIZE; dst++, src++, count++) {
        *dst = *src;
    }
}

uchar& Frame::operator[](const std::size_t idx) {
    return data[idx];
}

const uchar& Frame::operator[](const std::size_t idx) const {
    return data[idx];
}

const uchar* Frame::operator+(const uint rhs) const {
    return data+rhs;
}

quint32 Frame::getBits(const uint number)
{
    if(number > 32)
        return 0;

    quint32 byte_ini = nBit / 8;
    nBit += number;

    quint32 byte_end = nBit / 8;
    quint32 bit_end = nBit % 8;

    if (byte_end > MAXFRAMESIZE)
        return 0;

    quint32 output;
    if (byte_end == byte_ini)
        output = data[byte_end];
    else if (byte_end==byte_ini+1)
        output = (data[byte_end - 1] * 256) + data[byte_end];
    else
        output = data[byte_end - 2] * 256 * 256 + data[byte_end - 1] * 256 + data[byte_end];

    output = (output) >> (8 - bit_end);
    output = output & ((1 << number) - 1);

    return output;
}
