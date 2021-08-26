#ifndef NATIVE_SUBMENUBUTTON_H
#define NATIVE_SUBMENUBUTTON_H

struct NativeEntity_SubMenuButton : NativeEntityBase {
    float matX;
    float matY;
    float matZ;
    int state;
    float matXOff;
    float textY;
    float afterFlashTimer;
    float flashTimer;
    float textScale;
    int textAlpha;
    byte r;
    byte g;
    byte b;
    ushort text[64];
    MeshInfo *meshButton;
    MeshInfo *meshButtonH;
    byte setNewState;
    MatrixF matrix;
    MatrixF renderMatrix;
    byte symbol;
    byte flags;
    byte textureSymbols;
    byte useMeshH;
};

void SubMenuButton_Create(void *objPtr);
void SubMenuButton_Main(void *objPtr);

#endif // !NATIVE_SUBMENUBUTTON_H
