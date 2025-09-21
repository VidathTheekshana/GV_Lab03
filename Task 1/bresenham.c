#include <GLUT/glut.h>
#include <stdlib.h>

void drawPixel(int x, int y) {
    glBegin(GL_POINTS);
    glVertex2i(x, y);
    glEnd();
}

// Bresenham’s algorithm
void bresenhamLine(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        drawPixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

// Display function
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(0, 0, 0);

    // Case 1: slope > 1
    bresenhamLine(50, 50, 70, 120);

    // Case 2: slope < 0
    bresenhamLine(50, 50, 120, 20);

    glFlush();
}

void init2D() {
    glClearColor(1, 1, 1, 1); // background white
    gluOrtho2D(0, 200, 0, 200); // coordinate system
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize(400, 400);
    glutCreateWindow("Bresenham Line Drawing");
    init2D();
    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}
