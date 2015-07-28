#include "zbytearray.h"
#include "zlog.h"

static const u16 crc16table[256] = { 0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241, 0xC601, 0x06C0,
                                     0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440, 0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
                                     0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841, 0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1,
                                     0xDA81, 0x1A40, 0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41, 0x1400, 0xD4C1, 0xD581, 0x1540,
                                     0xD701, 0x17C0, 0x1680, 0xD641, 0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040, 0xF001, 0x30C0,
                                     0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240, 0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
                                     0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41, 0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1,
                                     0xF881, 0x3840, 0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41, 0xEE01, 0x2EC0, 0x2F80, 0xEF41,
                                     0x2D00, 0xEDC1, 0xEC81, 0x2C40, 0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640, 0x2200, 0xE2C1,
                                     0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041, 0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
                                     0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441, 0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0,
                                     0x6E80, 0xAE41, 0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840, 0x7800, 0xB8C1, 0xB981, 0x7940,
                                     0xBB01, 0x7BC0, 0x7A80, 0xBA41, 0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40, 0xB401, 0x74C0,
                                     0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640, 0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
                                     0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241, 0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1,
                                     0x9481, 0x5440, 0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40, 0x5A00, 0x9AC1, 0x9B81, 0x5B40,
                                     0x9901, 0x59C0, 0x5880, 0x9841, 0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40, 0x4E00, 0x8EC1,
                                     0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41, 0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
                                     0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040 };

static const u8 uuid[] = {
    0xff, 0x53, 0xbb, 0x1a,
    0x7d, 0xb7, 0x42, 0x5a,
    0x92, 0xad, 0xd3, 0xc1,
    0x88, 0x13, 0x11, 0xac
};

static void reverseData(void *data, int len) {
    int j = len / 2;
    for (int i = 0; i < j; i++) {
        u8 *p = (u8*) data + i;
        u8 *q = (u8*) data + (len - i - 1);
        u8 tmp = *p;
        *p = *q;
        *q = tmp;
    }
}

#define getValue(x) do {\
    if(i<0||i>=size()) {\
    x=0;\
    break;\
    }\
    char *__p = data() + i;\
    int __len = sizeof(x);\
    memcpy(&x, __p, __len);\
    if(reverse) {\
    reverseData(&x, __len);\
    }\
    i += __len;\
    } while(0)

#define putValue(x) do {\
    int __len = sizeof(x);\
    char *__tmp = new char[__len];\
    memcpy(__tmp, &x, __len);\
    if(reverse) {\
    reverseData(__tmp, __len);\
    }\
    append(__tmp, __len);\
    delete[] __tmp;\
    } while(0)

ZByteArray::ZByteArray(bool bigEndian) :
    QByteArray() {
    int t = 0x2b;
    unsigned char b = *((char *) &t);
    if (b == 0x2b) { // little endian
        reverse = bigEndian;
    } else {
        reverse = !bigEndian;
    }
}

u16 ZByteArray::checksum() {
    u16 ret = 0;
    int i = 0;
    int len = this->length();
    char *p = this->data();
    while (len-- > 0) {
        i = ((ret >> 8) ^ *p) & 0xff;
        ret = (ret << 8) ^ crc16table[i];
        p++;
    }
    return ret;
}

u16 ZByteArray::checksum(int from, int length) {
    int ret = 0;
    char *p = this->data();
    int i = from;
    int j;
    u8 b;
    for (; i < from + length; i++) {
        b = *(p+i);
        j = ((ret >> 8) ^ b) & 0xff;
        ret = (ret << 8) ^ crc16table[j];
    }
    return (u16) ret;
}

int ZByteArray::indexOf(void *data, int len) {
    QByteArray tmp((const char *)data, len);
    return QByteArray::indexOf(tmp);
}

u8 ZByteArray::getNextByte(int &i) {
    return at(i++);
}

u16 ZByteArray::getNextShort(int &i) {
    u16 ret;
    getValue(ret);
    return ret;
}

u32 ZByteArray::getNextInt(int &i) {
    u32 ret;
    getValue(ret);
    return ret;
}

u64 ZByteArray::getNextInt64(int &i) {
    u64 ret;
    getValue(ret);
    return ret;
}

QString ZByteArray::getNextUtf8(int &i) {
    int len = getNextInt(i);
    char *p = data() + i;
    QString ret = QString::fromUtf8(p, len);
    i += len;
    return ret;
}

QString ZByteArray::getNextUtf8(int &i, int len) {
    char *p = data() + i;
    QString ret = QString::fromUtf8(p, len);
    i += len;
    return ret;
}

QString ZByteArray::getNextUtf16(int &i) {
    QString ret;
    int len = getNextInt(i);
    for (int j = 0; j < len; j++) {
        u16 s;
        getValue(s);
        ret.append(QChar(s));
    }
    return ret;
}

QString ZByteArray::getNextUtf16(int &i, int len) {
    QString ret;
    for (int j = 0; j < len; j++) {
        u16 s;
        getValue(s);
        ret.append(QChar(s));
    }
    return ret;
}

void ZByteArray::putByte(u8 b) {
    append(b);
}

void ZByteArray::putShort(u16 s) {
    putValue(s);
}

void ZByteArray::putInt(u32 i32) {
    putValue(i32);
}

void ZByteArray::putInt64(u64 i64) {
    putValue(i64);
}

void ZByteArray::putUtf8(const QString &str, bool addHead) {
    QByteArray data = str.toUtf8();
    if(addHead) {
        int len = data.length();
        putValue(len);
    }
    append(data);
}

void ZByteArray::putUtf16(const QString &str, bool addHead) {
    if(addHead) {
        int len = str.length();
        putValue(len);
    }
    foreach(const QChar& c, str) {
        u16 u = c.unicode();
        putValue(u);
    }
}

// [modified content][ori_crc][now_crc]
void ZByteArray::encode(ZByteArray& out) {
    out.resize(this->size());
    char *p = this->data();
    char *q = out.data();
    u16 crc = this->checksum(0, this->size());
    int s = crc % 8;
    u8 a, b, tmp;
    for(int i=0; i<this->size(); i++) {
        tmp = *p++ ^ uuid[i%sizeof(uuid)];
        a = (tmp << (8-s)) & 0xff;
        b = (tmp >> s) & 0xff;
        tmp = a | b;
        *q++ = tmp;
        s = 8-s;
    }
    out.putShort(crc);
    crc = out.checksum(0, this->size());
    out.putShort(crc);
}

// [modified content][ori_crc][now_crc]
bool ZByteArray::decode(ZByteArray& out) {
    if(this->size() < 4) {
        return false;
    }
    int i = this->size()-4;
    out.resize(i);
    u16 calc_crc = this->checksum(0, i);
    u16 ori_crc = this->getNextShort(i);
    u16 read_crc = this->getNextShort(i);
    if(calc_crc != read_crc) {
        DBG("invalid enc_crc\n");
        return false;
    }

    char *p = this->data();
    char *q = out.data();
    int s = 8 - (ori_crc % 8);
    u8 a, b, tmp;

    for(i=0; i<out.size(); i++) {
        tmp = *p++;
        a = (tmp << (8-s)) & 0xff;
        b = (tmp >> s) & 0xff;
        tmp = a | b;
        *q++ = tmp ^ uuid[i%sizeof(uuid)];
        s = 8-s;
    }

    calc_crc = out.checksum(0, out.size());
    if(calc_crc != ori_crc) {
        DBG("invalid ori_crc\n");
        return false;
    }
    return true;
}

QString ZByteArray::encode(const QString& in) {
    ZByteArray inData(false);
    ZByteArray outData(false);
    inData.append(in.toUtf8());
    inData.encode(outData);
    return outData.toHex();
}

QString ZByteArray::decode(const QString& in) {
    ZByteArray inData(false);
    ZByteArray outData(false);
    inData.append(QByteArray::fromHex(in.toUtf8()));
    if(inData.size() > 4 && inData.decode(outData)) {
        return QString::fromUtf8(outData);
    }
    return QString();
}

void ZByteArray::encode_bytearray(QByteArray& in) {
#ifndef CMD_DEBUG
    ZByteArray inData(false);
    ZByteArray outData(false);
    inData.append(in);
    inData.encode(outData);
    in.clear();
    in.append(outData);
#endif
}

void ZByteArray::decode_bytearray(QByteArray& in) {
    ZByteArray inData(false);
    ZByteArray outData(false);
    inData.append(in);
    if(inData.size() > 4 && inData.decode(outData)) {
        in.clear();
        in.append(outData);
    }
}
