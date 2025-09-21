
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

typedef struct { unsigned int width, height; unsigned char *data; } BMPImage;

int loadBMP24(const char *path, BMPImage *out) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "BMP open failed: %s\n", path); return 0; }

    unsigned char header[54];
    if (fread(header, 1, 54, f) != 54) { fclose(f); return 0; }
    if (header[0] != 'B' || header[1] != 'M') { fclose(f); return 0; }

    unsigned int dataOffset = *(unsigned int*)&header[10];
    unsigned int size       = *(unsigned int*)&header[34];
    unsigned int w          = *(unsigned int*)&header[18];
    unsigned int h          = *(unsigned int*)&header[22];
    unsigned short bpp      = *(unsigned short*)&header[28];
    unsigned int compression= *(unsigned int*)&header[30];

    if (bpp != 24 || compression != 0) {
        fprintf(stderr, "Only 24-bit uncompressed BMP supported.\n");
        fclose(f); return 0;
    }
    if (size == 0) size = w * h * 3;

    unsigned char *bgr = (unsigned char*)malloc(size);
    fseek(f, dataOffset, SEEK_SET);
    if (fread(bgr, 1, size, f) != size) { free(bgr); fclose(f); return 0; }
    fclose(f);

    // Convert BGR→RGB and flip vertically
    unsigned char *rgb = (unsigned char*)malloc(size);
    for (unsigned int y = 0; y < h; ++y) {
        for (unsigned int x = 0; x < w; ++x) {
            unsigned int src = (y * w + x) * 3;
            unsigned int dst = ((h - 1 - y) * w + x) * 3;
            rgb[dst + 0] = bgr[src + 2];
            rgb[dst + 1] = bgr[src + 1];
            rgb[dst + 2] = bgr[src + 0];
        }
    }
    free(bgr);

    out->width = w; out->height = h; out->data = rgb;
    return 1;
}

typedef struct { float x,y,z; } Vec3;
typedef struct { float u,v;   } Vec2;
typedef struct {
    int   numFaces;     // triangles count
    float *positions;   // 3 floats per vertex
    float *texcoords;   // 2 floats per vertex
} Mesh;

static Vec3 *tmpV = NULL; int tmpVCount = 0, tmpVCap = 0;
static Vec2 *tmpVT= NULL; int tmpVTCount= 0, tmpVTCap= 0;

static void pushV(float x,float y,float z){
    if (tmpVCount == tmpVCap){ tmpVCap = tmpVCap? tmpVCap*2:1024; tmpV=(Vec3*)realloc(tmpV,tmpVCap*sizeof(Vec3)); }
    tmpV[tmpVCount++] = (Vec3){x,y,z};
}
static void pushVT(float u,float v){
    if (tmpVTCount== tmpVTCap){ tmpVTCap= tmpVTCap? tmpVTCap*2:1024; tmpVT=(Vec2*)realloc(tmpVT,tmpVTCap*sizeof(Vec2)); }
    tmpVT[tmpVTCount++] = (Vec2){u,v};
}

static int parseFaceVert(const char *tok, int *vi, int *ti) {
    int v=0, vt=0;
    if (sscanf(tok, "%d/%d", &v, &vt) == 2) { *vi = v-1; *ti = vt-1; return 1; }
    if (sscanf(tok, "%d//%*d", &v) == 1)   { *vi = v-1; *ti = -1;   return 1; }
    if (sscanf(tok, "%d", &v) == 1)        { *vi = v-1; *ti = -1;   return 1; }
    return 0;
}

int loadOBJ_WithUV_Triangulate(const char *path, Mesh *mesh) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "OBJ open failed: %s\n", path); return 0; }

    char line[1024]; int faceVertTotal = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0]=='v' && line[1]==' ') {
            float x,y,z; sscanf(line, "v %f %f %f", &x,&y,&z); pushV(x,y,z);
        } else if (line[0]=='v' && line[1]=='t') {
            float u,v;   sscanf(line, "vt %f %f", &u,&v);      pushVT(u,v);
        } else if (line[0]=='f' && line[1]==' ') {
            int count=0; char *p=line+2, tok[128];
            while (sscanf(p,"%127s",tok)==1){ count++; char *sp=strchr(p,' '); if(!sp)break; while(*sp==' ')sp++; p=sp; }
            if (count>=3) faceVertTotal += (count-2)*3;
        }
    }

    mesh->numFaces = faceVertTotal/3;
    mesh->positions = (float*)malloc(faceVertTotal*3*sizeof(float));
    mesh->texcoords = (float*)malloc(faceVertTotal*2*sizeof(float));

    rewind(f);
    int writeIdx=0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0]=='f' && line[1]==' ') {
            int idxV[64], idxT[64], n=0; char *p=line+2, tok[128];
            while (sscanf(p,"%127s",tok)==1) {
                int vi=-1, ti=-1; if(!parseFaceVert(tok,&vi,&ti)) break;
                idxV[n]=vi; idxT[n]=ti; n++;
                char *sp=strchr(p,' '); if(!sp)break; while(*sp==' ')sp++; p=sp;
                if (n>=64) break;
            }
            if (n>=3) {
                for (int i=1;i<n-1;++i) {
                    int tri[3]={0,i,i+1};
                    for (int k=0;k<3;++k) {
                        int vIdx=idxV[tri[k]], tIdx=idxT[tri[k]];
                        Vec3 v = (vIdx>=0 && vIdx<tmpVCount)? tmpV[vIdx] : (Vec3){0,0,0};
                        Vec2 t = (tIdx>=0 && tIdx<tmpVTCount)? tmpVT[tIdx]: (Vec2){0,0};
                        mesh->positions[(writeIdx*3)+0]=v.x;
                        mesh->positions[(writeIdx*3)+1]=v.y;
                        mesh->positions[(writeIdx*3)+2]=v.z;
                        mesh->texcoords[(writeIdx*2)+0]=t.u;
                        mesh->texcoords[(writeIdx*2)+1]=t.v;
                        writeIdx++;
                    }
                }
            }
        }
    }
    fclose(f);
    return 1;
}


GLuint gTexture = 0;
Mesh   gMesh    = {0};
float  gAngleY  = 0.0f;
int    lastX=0, dragging=0;

GLuint createTextureFromBMP(const char *bmpPath) {
    BMPImage img = {0};
    if (!loadBMP24(bmpPath, &img)) return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);   // classic bind (fixed pipeline)

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 img.width, img.height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, img.data);

    free(img.data);
    return tex;
}

void initGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    gTexture = createTextureFromBMP("donut_texture.bmp");
    if (!gTexture) fprintf(stderr, "Failed to load donut_texture.bmp\n");

    if (!loadOBJ_WithUV_Triangulate("donut.obj", &gMesh)) {
        fprintf(stderr, "Failed to load donut.obj\n");
        exit(1);
    }
    glClearColor(0.95f, 0.95f, 1.0f, 1.0f);
}

void reshape(int w, int h) {
    if (h==0) h=1;
    float aspect=(float)w/(float)h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, aspect, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void drawMesh(const Mesh *m) {
    glBindTexture(GL_TEXTURE_2D, gTexture);
    glBegin(GL_TRIANGLES);
    for (int i=0;i<m->numFaces*3;++i) {
        float u = m->texcoords[i*2+0];
        float v = m->texcoords[i*2+1];
        float x = m->positions[i*3+0];
        float y = m->positions[i*3+1];
        float z = m->positions[i*3+2];
        glTexCoord2f(u, v);
        glVertex3f(x, y, z);
    }
    glEnd();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();
  
    gluLookAt(0, 0, 8,   0, 0, 0,   0, 1, 0);

  
    glScalef(0.2f, 0.2f, 0.2f);  


    glRotatef(20.0f, 1, 0, 0);
    glRotatef(gAngleY, 0, 1, 0);

    drawMesh(&gMesh);
    glutSwapBuffers();
}

void idle() {
    gAngleY += 0.2f;
    if (gAngleY > 360.f) gAngleY -= 360.f;
    glutPostRedisplay();
}

// Simple mouse-drag to orbit Y
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        dragging = (state == GLUT_DOWN);
        lastX = x;
    }
}
void motion(int x, int y) {
    if (dragging) {
        int dx = x - lastX;
        gAngleY += dx * 0.5f;
        lastX = x;
        glutPostRedisplay();
    }
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(900, 700);
    glutCreateWindow("Textured Donut - OpenGL (GLUT)");

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    glutMainLoop();
    return 0;
}
