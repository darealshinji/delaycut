#ifndef FRAME_H
#define FRAME_H

#include <QObject>

#define MAXFRAMESIZE 16384

class Frame : public QObject
{
    Q_OBJECT
public:
    explicit Frame(QObject *parent = 0);
    const uchar* getData() const;
    void setData(const uchar* newData, const uint size);
    uchar& operator[](const std::size_t idx);
    const uchar& operator[](const std::size_t idx) const;
    const uchar* operator+(const uint rhs) const;
    quint32 getBits(const uint number);

private:
    uint nBit;
    uchar data[MAXFRAMESIZE];
};

#endif // FRAME_H
