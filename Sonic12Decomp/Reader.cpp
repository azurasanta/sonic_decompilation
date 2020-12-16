#include "RetroEngine.hpp"
#include <string>

RSDKContainer rsdkContainer;
char rsdkName[0x400];

char fileName[0x100];
byte fileBuffer[0x2000];
int fileSize;
int vFileSize;
int readPos;
int readSize;
int bufferPosition;
int virtualFileOffset;
bool useEncryption;
byte eStringPosA;
byte eStringPosB;
byte eStringNo;
byte eNybbleSwap;
byte encryptionStringA[0x10];
byte encryptionStringB[0x10];

FILE *cFileHandle = nullptr;
FILE *cFileHandleStream = nullptr;

bool CheckRSDKFile(const char *filePath)
{
    FileInfo info;

    Engine.usingDataFile = false;
    Engine.usingBytecode = false;

    // CopyFilePath(filename, &rsdkName);
    cFileHandle = fopen(filePath, "rb");
    if (cFileHandle) {
        byte signature[6] = { 'R', 'S', 'D', 'K', 'v', 'B' };
        byte buf          = 0;
        for (int i = 0; i < 6; ++i) {
            fread(&buf, 1, 1, cFileHandle);
            if (buf != signature[i])
                return false;
        }
        Engine.usingDataFile = true;
        StrCopy(rsdkName, filePath);

        rsdkContainer.fileCount = 0;
        fread(&rsdkContainer.fileCount, 2, 1, cFileHandle);
        for (int f = 0; f < rsdkContainer.fileCount; ++f) {
            for (int y = 0; y < 16; y += 4) {
                fread(&rsdkContainer.files[f].hash[y + 3], 1, 1, cFileHandle);
                fread(&rsdkContainer.files[f].hash[y + 2], 1, 1, cFileHandle);
                fread(&rsdkContainer.files[f].hash[y + 1], 1, 1, cFileHandle);
                fread(&rsdkContainer.files[f].hash[y + 0], 1, 1, cFileHandle);
            }

            fread(&rsdkContainer.files[f].offset, 4, 1, cFileHandle);
            fread(&rsdkContainer.files[f].filesize, 4, 1, cFileHandle);

            rsdkContainer.files[f].encrypted = (rsdkContainer.files[f].filesize & 0x80000000);
            rsdkContainer.files[f].filesize &= 0x7FFFFFFF;

            rsdkContainer.files[f].fileID = f;
        }

        fclose(cFileHandle);
        cFileHandle = NULL;
        if (LoadFile("ByteCode/GlobalCode.bin", &info)) {
            Engine.usingBytecode = true;
            CloseFile();
        }
        return true;
    }
    else {
        Engine.usingDataFile = false;
        cFileHandle = NULL;
        if (LoadFile("ByteCode/GlobalCode.bin", &info)) {
            Engine.usingBytecode = true;
            CloseFile();
        }
        return false;
    }

    return false;
}

bool LoadFile(const char *filePath, FileInfo *fileInfo)
{
    MEM_ZEROP(fileInfo);
    StringLowerCase(fileInfo->fileName, filePath);
    StrCopy(fileName, fileInfo->fileName);

    if (cFileHandle)
        fclose(cFileHandle);

    cFileHandle = NULL;
    if (Engine.usingDataFile) {
        byte buffer[0x10];
        int len = StrLength(fileInfo->fileName);
        GenerateMD5FromString(fileInfo->fileName, len, buffer);


        for (int f = 0; f < rsdkContainer.fileCount; ++f) {
            RSDKFileInfo *file = &rsdkContainer.files[f];

            bool match = true;
            for (int h = 0; h < 0x10; ++h) {
                if (buffer[h] != file->hash[h]) {
                    match = false;
                    break;
                }
            }
            if (!match)
                continue;

            cFileHandle = fopen(rsdkName, "rb");
            fseek(cFileHandle, 0, SEEK_END);
            fileSize       = (int)ftell(cFileHandle);

            vFileSize = file->filesize;
            virtualFileOffset = file->offset;
            readPos           = file->offset;
            readSize          = 0;
            bufferPosition    = 0;
            fseek(cFileHandle, virtualFileOffset, SEEK_SET);

            useEncryption = file->encrypted;
            if (useEncryption) {
                GenerateELoadKeys(vFileSize, (vFileSize >> 1) + 1);
                eStringNo   = (vFileSize & 0x1FC) >> 2;
                eStringPosA = 0;
                eStringPosB = 8;
                eNybbleSwap = 0;
            }
            
            fileInfo->readPos           = readPos;
            fileInfo->fileSize          = fileSize;
            fileInfo->vfileSize         = vFileSize;
            fileInfo->virtualFileOffset = virtualFileOffset;
            fileInfo->eStringNo         = eStringNo;
            fileInfo->eStringPosB       = eStringPosB;
            fileInfo->eStringPosA       = eStringPosA;
            fileInfo->eNybbleSwap       = eNybbleSwap;
            fileInfo->bufferPosition    = bufferPosition;
            fileInfo->useEncryption     = useEncryption;
#if RSDK_DEBUG
            printf("Loaded File '%s'\n", filePath);
#endif
            return true;
        }
#if RSDK_DEBUG
        printf("Couldn't load file '%s'\n", filePath);
#endif
        return false;
    }
    else {
        cFileHandle = fopen(fileInfo->fileName, "rb");
        if (!cFileHandle) {
#if RSDK_DEBUG
            printf("Couldn't load file '%s'\n", filePath);
#endif
            return false;
        }
        virtualFileOffset = 0;
        fseek(cFileHandle, 0, SEEK_END);
        fileInfo->fileSize = (int)ftell(cFileHandle);
        fileSize = fileInfo->vfileSize = fileInfo->fileSize;
        fseek(cFileHandle, 0, SEEK_SET);
        readPos = 0;
        fileInfo->readPos           = readPos;
        fileInfo->virtualFileOffset = 0;
        fileInfo->eStringNo         = 0;
        fileInfo->eStringPosB       = 0;
        fileInfo->eStringPosA       = 0;
        fileInfo->eNybbleSwap       = 0;
        fileInfo->bufferPosition    = 0;
        bufferPosition              = 0;
        readSize                    = 0;

#if RSDK_DEBUG
        printf("Loaded File '%s'\n", filePath);
#endif

        return true;
    }
}

void GenerateELoadKeys(uint key1, uint key2)
{
    char buffer[0x20];
    byte hash[0x10];

    //StringA
    ConvertIntegerToString(buffer, key1);
    int len = StrLength(buffer);
    GenerateMD5FromString(buffer, len, hash);

    for (int y = 0; y < 0x10; y += 4) {
        encryptionStringA[y + 3] = hash[y + 0];
        encryptionStringA[y + 2] = hash[y + 1];
        encryptionStringA[y + 1] = hash[y + 2];
        encryptionStringA[y + 0] = hash[y + 3];
    }

    // StringB
    ConvertIntegerToString(buffer, key2);
    len = StrLength(buffer);
    GenerateMD5FromString(buffer, len, hash);

    for (int y = 0; y < 0x10; y += 4) {
        encryptionStringB[y + 3] = hash[y + 0];
        encryptionStringB[y + 2] = hash[y + 1];
        encryptionStringB[y + 1] = hash[y + 2];
        encryptionStringB[y + 0] = hash[y + 3];
    }

}

const uint ENC_KEY_2 = 0x24924925;
const uint ENC_KEY_1 = 0xAAAAAAAB;
int mulUnsignedHigh(uint arg1, int arg2) { return (int)(((ulong)arg1 * (ulong)arg2) >> 32); }

void FileRead(void *dest, int size)
{
    byte *data = (byte *)dest;

    if (readPos <= fileSize) {
        if (useEncryption) {
            while (size > 0) {
                if (bufferPosition == readSize)
                    FillFileBuffer();

                *data = encryptionStringB[eStringPosB] ^ eStringNo ^ fileBuffer[bufferPosition++];
                if (eNybbleSwap)
                    *data = ((*data << 4) + (*data >> 4)) & 0xFF;
                *data ^= encryptionStringA[eStringPosA];
                
                ++eStringPosA;
                ++eStringPosB;
                if (eStringPosA <= 0x0F) {
                    if (eStringPosB > 0x0C) {
                        eStringPosB = 0;
                        eNybbleSwap ^= 0x01;
                    }
                }
                else if (eStringPosB <= 0x08) {
                    eStringPosA = 0;
                    eNybbleSwap ^= 0x01;
                }
                else {
                    eStringNo += 2;
                    eStringNo &= 0x7F;

                    if (eNybbleSwap != 0) {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        eNybbleSwap = 0;

                        int temp1 = key2 + (eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        eStringPosA = eStringNo - temp1 / 4 * 7;
                        eStringPosB = eStringNo - temp2 * 4 + 2;
                    }
                    else {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        eNybbleSwap = 1;

                        int temp1 = key2 + (eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        eStringPosB = eStringNo - temp1 / 4 * 7;
                        eStringPosA = eStringNo - temp2 * 4 + 3;
                    }
                }

                ++data;
                --size;
            }
        }
        else {
            while (size > 0) {
                if (bufferPosition == readSize)
                    FillFileBuffer();

                *data++ = fileBuffer[bufferPosition++];
                size--;
            }
        }
    }
}

void SetFileInfo(FileInfo *fileInfo)
{
    if (Engine.usingDataFile) {
        cFileHandle       = fopen(rsdkName, "rb");
        virtualFileOffset = fileInfo->virtualFileOffset;
        vFileSize         = fileInfo->vfileSize;
        fseek(cFileHandle, 0, SEEK_END);
        fileSize = (int)ftell(cFileHandle);
        readPos  = fileInfo->readPos;
        fseek(cFileHandle, readPos, SEEK_SET);
        FillFileBuffer();
        bufferPosition = fileInfo->bufferPosition;
        eStringPosA    = fileInfo->eStringPosA;
        eStringPosB    = fileInfo->eStringPosB;
        eStringNo      = fileInfo->eStringNo;
        eNybbleSwap    = fileInfo->eNybbleSwap;
        useEncryption  = fileInfo->useEncryption;

        if (useEncryption) {
            GenerateELoadKeys(vFileSize, (vFileSize >> 1) + 1);
        }
    }
    else {
        StrCopy(fileName, fileInfo->fileName);
        cFileHandle       = fopen(fileInfo->fileName, "rb");
        virtualFileOffset = 0;
        fileSize          = fileInfo->fileSize;
        readPos           = fileInfo->readPos;
        fseek(cFileHandle, readPos, SEEK_SET);
        FillFileBuffer();
        bufferPosition = fileInfo->bufferPosition;
        eStringPosA    = 0;
        eStringPosB    = 0;
        eStringNo      = 0;
        eNybbleSwap    = 0;
    }
}

size_t GetFilePosition()
{
    if (Engine.usingDataFile)
        return bufferPosition + readPos - readSize - virtualFileOffset;
    else
        return bufferPosition + readPos - readSize;
}

void SetFilePosition(int newPos)
{
    if (useEncryption) {
        readPos     = virtualFileOffset + newPos;
        eStringNo   = (vFileSize & 0x1FC) >> 2;
        eStringPosA = 0;
        eStringPosB = 8;
        eNybbleSwap = false;
        while (newPos) {
            ++eStringPosA;
            ++eStringPosB;
            if (eStringPosA <= 0x0F) {
                if (eStringPosB > 0x0C) {
                    eStringPosB = 0;
                    eNybbleSwap ^= 0x01;
                }
            }
            else if (eStringPosB <= 0x08) {
                eStringPosA = 0;
                eNybbleSwap ^= 0x01;
            }
            else {
                eStringNo += 2;
                eStringNo &= 0x7F;

                if (eNybbleSwap != 0) {
                    int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                    int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                    eNybbleSwap = 0;

                    int temp1 = key2 + (eStringNo - key2) / 2;
                    int temp2 = key1 / 8 * 3;

                    eStringPosA = eStringNo - temp1 / 4 * 7;
                    eStringPosB = eStringNo - temp2 * 4 + 2;
                }
                else {
                    int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                    int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                    eNybbleSwap = 1;

                    int temp1 = key2 + (eStringNo - key2) / 2;
                    int temp2 = key1 / 8 * 3;

                    eStringPosB = eStringNo - temp1 / 4 * 7;
                    eStringPosA = eStringNo - temp2 * 4 + 3;
                }
            }
            --newPos;
        }
    }
    else {
        readPos = newPos;
    }
    fseek(cFileHandle, readPos, SEEK_SET);
    FillFileBuffer();
}

bool ReachedEndOfFile()
{
    if (Engine.usingDataFile)
        return bufferPosition + readPos - readSize - virtualFileOffset >= vFileSize;
    else
        return bufferPosition + readPos - readSize >= fileSize;
}

size_t FileRead2(FileInfo *info, void *dest, int size)
{
    byte *data = (byte *)dest;
    int rPos   = (int)GetFilePosition2(info);
    memset(data, 0, size);

    if (rPos <= info->vfileSize) {
        if (useEncryption) {
            int rSize = 0;
            if (rPos + size <= info->vfileSize)
                rSize = size;
            else
                rSize = info->vfileSize - rPos;

            size_t result = fread(data, 1u, rSize, cFileHandleStream);
            info->readPos += rSize;
            info->bufferPosition = 0;

            while (size > 0) {
                *data = encryptionStringB[info->eStringPosB] ^ info->eStringNo ^ *data;
                if (info->eNybbleSwap)
                    *data = 16 * (*data & 0xF) + (*data >> 4);
                *data ^= encryptionStringA[info->eStringPosA];

                ++info->eStringPosA;
                ++info->eStringPosB;
                if (info->eStringPosA <= 0x0F) {
                    if (info->eStringPosB > 0x0C) {
                        info->eStringPosB = 0;
                        info->eNybbleSwap ^= 0x01;
                    }
                }
                else if (info->eStringPosB <= 0x08) {
                    info->eStringPosA = 0;
                    info->eNybbleSwap ^= 0x01;
                }
                else {
                    info->eStringNo += 2;
                    info->eStringNo &= 0x7F;

                    if (info->eNybbleSwap != 0) {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        info->eNybbleSwap = 0;

                        int temp1 = key2 + (info->eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        info->eStringPosA = info->eStringNo - temp1 / 4 * 7;
                        info->eStringPosB = info->eStringNo - temp2 * 4 + 2;
                    }
                    else {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        info->eNybbleSwap = 1;

                        int temp1 = key2 + (info->eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        info->eStringPosB = info->eStringNo - temp1 / 4 * 7;
                        info->eStringPosA = info->eStringNo - temp2 * 4 + 3;
                    }
                }
                ++data;
                --size;
            }
            return result;
        }
        else {
            int rSize = 0;
            if (rPos + size <= info->vfileSize)
                rSize = size;
            else
                rSize = info->vfileSize - rPos;

            size_t result = fread(data, 1u, rSize, cFileHandleStream);
            info->readPos += rSize;
            info->bufferPosition = 0;
            return result;
        }
    }
    return 0;
}

size_t GetFilePosition2(FileInfo* info)
{
    if (Engine.usingDataFile)
        return info->bufferPosition + info->readPos - info->virtualFileOffset;
    else
        return info->bufferPosition + info->readPos;
}

void SetFilePosition2(FileInfo *info, int newPos)
{
    if (Engine.usingDataFile) {
        info->readPos     = info->virtualFileOffset + newPos;
        info->eStringNo   = (info->vfileSize & 0x1FCu) >> 2;
        info->eStringPosB = (info->eStringNo % 9) + 1;
        info->eStringPosA = (info->eStringNo % info->eStringPosB) + 1;
        info->eNybbleSwap = false;
        while (newPos) {
            ++info->eStringPosA;
            ++info->eStringPosB;
            if (info->eStringPosA <= 0x0F) {
                if (info->eStringPosB > 0x0C) {
                    info->eStringPosB = 0;
                    info->eNybbleSwap ^= 0x01;
                }
            }
            else if (info->eStringPosB <= 0x08) {
                info->eStringPosA = 0;
                info->eNybbleSwap ^= 0x01;
            }
            else {
                info->eStringNo += 2;
                info->eStringNo &= 0x7F;

                if (info->eNybbleSwap != 0) {
                    int key1          = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                    int key2          = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                    info->eNybbleSwap = 0;

                    int temp1 = key2 + (info->eStringNo - key2) / 2;
                    int temp2 = key1 / 8 * 3;

                    info->eStringPosA = info->eStringNo - temp1 / 4 * 7;
                    info->eStringPosB = info->eStringNo - temp2 * 4 + 2;
                }
                else {
                    int key1          = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                    int key2          = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                    info->eNybbleSwap = 1;

                    int temp1 = key2 + (info->eStringNo - key2) / 2;
                    int temp2 = key1 / 8 * 3;

                    info->eStringPosB = info->eStringNo - temp1 / 4 * 7;
                    info->eStringPosA = info->eStringNo - temp2 * 4 + 3;
                }
            }
            --newPos;
        }
    }
    else {
        info->readPos = newPos;
    }
    fseek(cFileHandleStream, info->readPos, SEEK_SET);
}
