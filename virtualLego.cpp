////////////////////////////////////////////////////////////////////////////////
//
// File: virtualLego.cpp
//
// Original Author: 박창현 Chang-hyeon Park, 
// Modified by Bong-Soo Sohn and Dong-Jun Kim
// 
// Originally programmed for Virtual LEGO. 
// Modified later to program for Virtual Billiard.
//        
////////////////////////////////////////////////////////////////////////////////

#include "d3dUtility.h"
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cassert>

// 디버깅 시 해제해 주세요
// #include <iostream>

IDirect3DDevice9* Device = NULL;
ID3DXFont* g_pFont = NULL; // 점수 표시용 폰트 객체 추가
bool showGuideLine = true;   // true면 조준선 표시

const float TABLE_X_MIN = -4.56f + 0.12f;
const float TABLE_X_MAX = 4.56f - 0.12f;
const float TABLE_Z_MIN = -3.06f + 0.12f;
const float TABLE_Z_MAX = 3.06f - 0.12f;
const float TABLE_Y = -0.00012f;

struct LineVertex {
    D3DXVECTOR3 pos;
    D3DCOLOR color;
};
#define D3DFVF_LINEVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE)

// window size
const int Width = 1024;
const int Height = 768;

// forward declaration for CSphere
class CSphere;

// Global Variable By Us
bool isTurnStarted = false;
int isWhiteTurn = 1; // 하얀공부터 시작하는 걸로
int whiteScore = 0;
int yellowScore = 0;
int winScore = 10;
int winner = 0;

bool isInitBlue = false;

CSphere* gs; // 포인터 선언만 가능 -> 이후에 g_sphere 배열 가리킬 예정.
CSphere* blue; // 선언 문제 -> g_sphere_blueball 가리킬 예정

// There are four balls
// initialize the position (coordinate) of each ball (ball0 ~ ball3)
const float spherePos[4][2] = { {-2.7f,0} , {+2.4f,0} , {-2.7f,-0.9f} , {3.3f, 0} };
// initialize the color of each ball (ball0 ~ ball3)
const D3DXCOLOR sphereColor[4] = { d3d::RED, d3d::RED, d3d::YELLOW, d3d::WHITE };

// -----------------------------------------------------------------------------
// Transform matrices
// -----------------------------------------------------------------------------
D3DXMATRIX g_mWorld;
D3DXMATRIX g_mView;
D3DXMATRIX g_mProj;

#define M_RADIUS 0.21   // ball radius
#define PI 3.14159265
#define M_HEIGHT 0.01
#define DECREASE_RATE 0.9982

// -----------------------------------------------------------------------------
// CSphere class definition
// -----------------------------------------------------------------------------

class CSphere {   // CSphere 클래스
private:
    float					center_x, center_y, center_z;
    float                   m_radius;
    float					m_velocity_x;
    float					m_velocity_z;
    bool                    hit[4];

public:

    CSphere(void)
    {
        D3DXMatrixIdentity(&m_mLocal);
        ZeroMemory(&m_mtrl, sizeof(m_mtrl));
        m_radius = 0;
        m_velocity_x = 0;
        m_velocity_z = 0;
        m_pSphereMesh = NULL;
    }
    ~CSphere(void) {}

public:
    bool create(IDirect3DDevice9* pDevice, D3DXCOLOR color = d3d::WHITE)
    {
        if (NULL == pDevice)
            return false;

        m_mtrl.Ambient = color;
        m_mtrl.Diffuse = color;
        m_mtrl.Specular = color;
        m_mtrl.Emissive = d3d::BLACK;
        m_mtrl.Power = 5.0f;

        if (FAILED(D3DXCreateSphere(pDevice, getRadius(), 50, 50, &m_pSphereMesh, NULL)))
            return false;
        return true;
    }

    void destroy(void)
    {
        if (m_pSphereMesh != NULL) {
            m_pSphereMesh->Release();
            m_pSphereMesh = NULL;
        }
    }

    void draw(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld)
    {
        if (NULL == pDevice)
            return;
        pDevice->SetTransform(D3DTS_WORLD, &mWorld);
        pDevice->MultiplyTransform(D3DTS_WORLD, &m_mLocal);
        pDevice->SetMaterial(&m_mtrl);
        m_pSphereMesh->DrawSubset(0);
    }

    bool hasIntersected(CSphere& ball)
    {
        D3DXVECTOR3 c1 = this->getCenter();
        D3DXVECTOR3 c2 = ball.getCenter();

        float dx = c1.x - c2.x;
        float dz = c1.z - c2.z;

        float distance = sqrt(dx * dx + dz * dz);
        float radiusSum = this->getRadius() + ball.getRadius();



        // 충돌 시 변수 업데이트
        // 상대 ball 도 바꿔야 함.
        if (distance <= radiusSum) {
            if (this == &(gs[0])) {
                ball.hit[0] = true;
            }
            else if (this == &(gs[1])) {
                ball.hit[1] = true;
            }
            else if (this == &(gs[2])) {
                ball.hit[2] = true;
            }
            else if (this == &(gs[3])) {
                ball.hit[3] = true;
            }

            if (&ball == &(gs[0])) {
                this->hit[0] = true; //Red1
            }
            else if (&ball == &(gs[1])) {
                this->hit[1] = true; //Red2
            }
            else if (&ball == &(gs[2])) {
                this->hit[2] = true; //Yellow
            }
            else if (&ball == &(gs[3])) {
                this->hit[3] = true; //White
            }
        }

        return distance <= radiusSum;
    }

    void hitBy(CSphere& ball)
    {
        if (!hasIntersected(ball)) return;

        // 중심 벡터 및 거리
        D3DXVECTOR3 c1 = this->getCenter();
        D3DXVECTOR3 c2 = ball.getCenter();
        D3DXVECTOR3 n = c1 - c2;  // 충돌 방향
        D3DXVec3Normalize(&n, &n);

        // 상대 속도
        D3DXVECTOR3 v1(this->getVelocity_X(), 0, this->getVelocity_Z());
        D3DXVECTOR3 v2(ball.getVelocity_X(), 0, ball.getVelocity_Z());
        D3DXVECTOR3 relVel = v1 - v2;

        // 두 공이 서로 멀어지는 중이면 무시
        if (D3DXVec3Dot(&relVel, &n) > 0)
            return;

        // 반사계수 e = 1 (완전탄성)
        float e = 1.0f;

        // 질량이 같을 때 단순화된 속도 교환 공식
        float v1n = D3DXVec3Dot(&v1, &n);
        float v2n = D3DXVec3Dot(&v2, &n);

        float p = (v1n - v2n);

        v1 -= n * p;
        v2 += n * p;

        this->setPower(v1.x, v1.z);
        ball.setPower(v2.x, v2.z);

        // --- 살짝 겹쳐진 공 위치 보정 (안 겹치게 밀기) ---
        float dist = D3DXVec3Length(&(c1 - c2));
        float overlap = (this->getRadius() + ball.getRadius() - dist) * 0.5f;
        if (overlap > 0)
        {
            D3DXVECTOR3 correction = n * overlap;
            this->setCenter(c1.x + correction.x, c1.y, c1.z + correction.z);
            ball.setCenter(c2.x - correction.x, c2.y, c2.z - correction.z);
        }
    }

    void ballUpdate(float timeDiff)
    {
        const float TIME_SCALE = 3.3;
        D3DXVECTOR3 cord = this->getCenter();
        double vx = abs(this->getVelocity_X());
        double vz = abs(this->getVelocity_Z());

        if (vx > 0.01 || vz > 0.01)
        {
            float tX = cord.x + TIME_SCALE * timeDiff * m_velocity_x;
            float tZ = cord.z + TIME_SCALE * timeDiff * m_velocity_z;

            //correction of position of ball
            // Please uncomment this part because this correction of ball position is necessary when a ball collides with a wall
            if (tX >= (4.5 - M_RADIUS))
                tX = 4.5 - M_RADIUS;
            else if (tX <= (-4.5 + M_RADIUS))
                tX = -4.5 + M_RADIUS;
            else if (tZ <= (-3 + M_RADIUS))
                tZ = -3 + M_RADIUS;
            else if (tZ >= (3 - M_RADIUS))
                tZ = 3 - M_RADIUS;

            this->setCenter(tX, cord.y, tZ);
        }
        else { this->setPower(0, 0); }
        //this->setPower(this->getVelocity_X() * DECREASE_RATE, this->getVelocity_Z() * DECREASE_RATE);
        double rate = 1 - (1 - DECREASE_RATE) * timeDiff * 400;
        if (rate < 0)
            rate = 0;
        this->setPower(getVelocity_X() * rate, getVelocity_Z() * rate);
    }

    double getVelocity_X() { return this->m_velocity_x; }
    double getVelocity_Z() { return this->m_velocity_z; }

    void setPower(double vx, double vz)
    {
        this->m_velocity_x = vx;
        this->m_velocity_z = vz;
    }

    void setCenter(float x, float y, float z)
    {
        D3DXMATRIX m;
        center_x = x;	center_y = y;	center_z = z;
        D3DXMatrixTranslation(&m, x, y, z);
        setLocalTransform(m);
    }

    float getRadius(void)  const { return (float)(M_RADIUS); }
    const D3DXMATRIX& getLocalTransform(void) const { return m_mLocal; }
    void setLocalTransform(const D3DXMATRIX& mLocal) { m_mLocal = mLocal; }
    D3DXVECTOR3 getCenter(void) const
    {
        D3DXVECTOR3 org(center_x, center_y, center_z);
        return org;
    }

    /*
    공별로 점수를 계산하는 함수. 실행 시에는 흰 공(player 1)과 노란 공(player 2)만을 대상으로 고려하면 됨.
    Rule: 흰 공, 또는 노란 공이 1. 상대방의 공을 건드렸거나, 빨간 공을 하나도 못 쳤을 경우 -1점 2. NOT 1이면서 빨간 공을 한개만 친 경우 0점. 3. NOT 1이면서 빨간 공을 두개 모두 친 경우 +1점.
    total_score>0일 시 턴 유지, 그렇지 않은 경우 턴 토글.
    ball number 0: r, 1: r, 2: y, 3: w
    */
    int getScore() {

        int total_score = 0;

        switch (isWhiteTurn) {
            // player 1's turn
        case (1):
            // case 1
            if (this->hit[2] == true) {
                total_score = -1;
            }
            else if (this->hit[0] == false && this->hit[1] == false) {
                total_score = -1;
            }
            // case 2
            else if ((this->hit[0] == true && this->hit[1] == false) || (this->hit[1] == true && this->hit[0] == false)) {
                total_score = 0;
            }
            // case 3
            else if ((this->hit[0] && this->hit[1]) == true) {
                total_score = 1;
            }
            break;

            // player 2's turn
        case (-1):
            // case 1
            if (this->hit[3] == true) {
                total_score = -1;
            }
            else if (this->hit[0] == false && this->hit[1] == false) {
                total_score = -1;
            }
            // case 2
            else if ((this->hit[0] == true && this->hit[1] == false) || (this->hit[1] == true && this->hit[0] == false)) {
                total_score = 0;
            }
            // case 3
            else if ((this->hit[0] && this->hit[1]) == true) {
                total_score = 1;
            }
            break;

            // unexpected value for isWhiteTurn.
        default:
            break;
        }

        return total_score;
    }
    void hit_initialize() {
        for (int i = 0; i < 4; i++) {
            hit[i] = false;
        }
    }

    bool* getHit() {
        return hit;
    }

private:
    D3DXMATRIX              m_mLocal;
    D3DMATERIAL9            m_mtrl;
    ID3DXMesh* m_pSphereMesh;

};



// -----------------------------------------------------------------------------
// CWall class definition
// -----------------------------------------------------------------------------

class CWall {

private:

    float					m_x;
    float					m_z;
    float                   m_width;
    float                   m_depth;
    float					m_height;

public:
    CWall(void)
    {
        D3DXMatrixIdentity(&m_mLocal);
        ZeroMemory(&m_mtrl, sizeof(m_mtrl));
        m_width = 0;
        m_depth = 0;
        m_pBoundMesh = NULL;
    }
    ~CWall(void) {}
public:
    bool create(IDirect3DDevice9* pDevice, float ix, float iz, float iwidth, float iheight, float idepth, D3DXCOLOR color = d3d::WHITE)
    {
        if (NULL == pDevice)
            return false;

        m_mtrl.Ambient = color;
        m_mtrl.Diffuse = color;
        m_mtrl.Specular = color;
        m_mtrl.Emissive = d3d::BLACK;
        m_mtrl.Power = 5.0f;

        m_width = iwidth;
        m_depth = idepth;

        if (FAILED(D3DXCreateBox(pDevice, iwidth, iheight, idepth, &m_pBoundMesh, NULL)))
            return false;
        return true;
    }
    void destroy(void)
    {
        if (m_pBoundMesh != NULL) {
            m_pBoundMesh->Release();
            m_pBoundMesh = NULL;
        }
    }
    void draw(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld)
    {
        if (NULL == pDevice)
            return;
        pDevice->SetTransform(D3DTS_WORLD, &mWorld);
        pDevice->MultiplyTransform(D3DTS_WORLD, &m_mLocal);
        pDevice->SetMaterial(&m_mtrl);
        m_pBoundMesh->DrawSubset(0);
    }

    bool hasIntersected(CSphere& ball)
    {
        D3DXVECTOR3 center = ball.getCenter();
        float r = ball.getRadius();

        // 예시: 위쪽 벽 (z가 +3 근처)
        if (fabs(m_z) > 0 && m_z > 0) { // 위쪽 벽이라면
            if (center.z + r >= m_z - (m_depth / 2))
                return true;
        }
        // 아래쪽 벽
        else if (fabs(m_z) > 0 && m_z < 0) {
            if (center.z - r <= m_z + (m_depth / 2))
                return true;
        }
        // 오른쪽 벽
        else if (fabs(m_x) > 0 && m_x > 0) {
            if (center.x + r >= m_x - (m_width / 2))
                return true;
        }
        // 왼쪽 벽
        else if (fabs(m_x) > 0 && m_x < 0) {
            if (center.x - r <= m_x + (m_width / 2))
                return true;
        }

        return false;
    }

    void hitBy(CSphere& ball)
    {
        if (!hasIntersected(ball)) return;

        // 공의 현재 속도
        double vx = ball.getVelocity_X();
        double vz = ball.getVelocity_Z();

        // 벽이 어느 방향에 있는가에 따라 반사
        if (fabs(m_z) > 0)  // 위/아래 벽
            ball.setPower(vx, -vz);
        else if (fabs(m_x) > 0) // 좌/우 벽
            ball.setPower(-vx, vz);
    }

    void setPosition(float x, float y, float z)
    {
        D3DXMATRIX m;
        this->m_x = x;
        this->m_z = z;

        D3DXMatrixTranslation(&m, x, y, z);
        setLocalTransform(m);
    }

    float getHeight(void) const { return M_HEIGHT; }



private:
    void setLocalTransform(const D3DXMATRIX& mLocal) { m_mLocal = mLocal; }

    D3DXMATRIX              m_mLocal;
    D3DMATERIAL9            m_mtrl;
    ID3DXMesh* m_pBoundMesh;
};

// -----------------------------------------------------------------------------
// CLight class definition
// -----------------------------------------------------------------------------

class CLight {
public:
    CLight(void)
    {
        static DWORD i = 0;
        m_index = i++;
        D3DXMatrixIdentity(&m_mLocal);
        ::ZeroMemory(&m_lit, sizeof(m_lit));
        m_pMesh = NULL;
        m_bound._center = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        m_bound._radius = 0.0f;
    }
    ~CLight(void) {}
public:
    bool create(IDirect3DDevice9* pDevice, const D3DLIGHT9& lit, float radius = 0.1f)
    {
        if (NULL == pDevice)
            return false;
        if (FAILED(D3DXCreateSphere(pDevice, radius, 10, 10, &m_pMesh, NULL)))
            return false;

        m_bound._center = lit.Position;
        m_bound._radius = radius;

        m_lit.Type = lit.Type;
        m_lit.Diffuse = lit.Diffuse;
        m_lit.Specular = lit.Specular;
        m_lit.Ambient = lit.Ambient;
        m_lit.Position = lit.Position;
        m_lit.Direction = lit.Direction;
        m_lit.Range = lit.Range;
        m_lit.Falloff = lit.Falloff;
        m_lit.Attenuation0 = lit.Attenuation0;
        m_lit.Attenuation1 = lit.Attenuation1;
        m_lit.Attenuation2 = lit.Attenuation2;
        m_lit.Theta = lit.Theta;
        m_lit.Phi = lit.Phi;
        return true;
    }
    void destroy(void)
    {
        if (m_pMesh != NULL) {
            m_pMesh->Release();
            m_pMesh = NULL;
        }
    }
    bool setLight(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld)
    {
        if (NULL == pDevice)
            return false;

        D3DXVECTOR3 pos(m_bound._center);
        D3DXVec3TransformCoord(&pos, &pos, &m_mLocal);
        D3DXVec3TransformCoord(&pos, &pos, &mWorld);
        m_lit.Position = pos;

        pDevice->SetLight(m_index, &m_lit);
        pDevice->LightEnable(m_index, TRUE);
        return true;
    }

    void draw(IDirect3DDevice9* pDevice)
    {
        if (NULL == pDevice)
            return;
        D3DXMATRIX m;
        D3DXMatrixTranslation(&m, m_lit.Position.x, m_lit.Position.y, m_lit.Position.z);
        pDevice->SetTransform(D3DTS_WORLD, &m);
        pDevice->SetMaterial(&d3d::WHITE_MTRL);
        m_pMesh->DrawSubset(0);
    }

    D3DXVECTOR3 getPosition(void) const { return D3DXVECTOR3(m_lit.Position); }

private:
    DWORD               m_index;
    D3DXMATRIX          m_mLocal;
    D3DLIGHT9           m_lit;
    ID3DXMesh* m_pMesh;
    d3d::BoundingSphere m_bound;
};

// -----------------------------------------------------------------------------
// Algorithms
// -----------------------------------------------------------------------------

// 상대 좌표 기반 상태
struct State {
    int dx1, dz1; // red1 - yellow 상대 위치
    int dx2, dz2; // red2 - yellow 상대 위치
    int dxw, dzw; // white - yellow 상대 위치
    int tx, tz; // 파란공 - yellow 상대 위치
};

// Q-learning 용 엔트리
struct QEntry {
    State state;        // 상태
    float totalReward;  // 누적 보상
    int count;          // 시도 횟수
    float avgReward;    // 평균 보상
};

// global variables for algorithms
std::vector<QEntry> QTable;
State lastState;

// functions of algorithms
int bin(float v, float step = 0.5f) {
    // 0.5 단위로 좌표를 정수화
    return int(v / step);
}

State getCurrentState() {   // 현재 상태 계산 함수
    D3DXVECTOR3 red1 = gs[0].getCenter();
    D3DXVECTOR3 red2 = gs[1].getCenter();
    D3DXVECTOR3 white = gs[3].getCenter();
    D3DXVECTOR3 yellow = gs[2].getCenter();
    D3DXVECTOR3 target = blue->getCenter();

    State s;
    s.dx1 = bin(red1.x - yellow.x);
    s.dz1 = bin(red1.z - yellow.z);
    s.dx2 = bin(red2.x - yellow.x);
    s.dz2 = bin(red2.z - yellow.z);
    s.dxw = bin(white.x - yellow.x);
    s.dzw = bin(white.z - yellow.z);
    s.tx = bin(target.x - yellow.x);
    s.tz = bin(target.z - yellow.z);
    return s;
}

void UpdateQTable(std::vector<QEntry>& qTable, const State& s, float reward) {   // QTable 갱신 함수
    for (auto& e : qTable) {
        if (memcmp(&e.state, &s, sizeof(State)) == 0) {
            // 기존 state 발견 → 업데이트
            e.totalReward += reward;
            e.count++;
            e.avgReward = e.totalReward / e.count;
            return;
        }
    }

    // 새로운 state 추가
    QEntry entry;
    entry.state = s;
    entry.totalReward = reward;
    entry.count = 1;
    entry.avgReward = reward;
    qTable.push_back(entry);
}

// Parameter File Load/Save
void SaveQTable(const std::vector<QEntry>& qTable) {
    FILE* fp = fopen("ai_qtable.txt", "w");
    if (!fp) return;
    for (auto& e : qTable) {
        fprintf(fp, "%d %d %d %d %d %d %d %d %f %d %f\n",
            e.state.dx1, e.state.dz1,
            e.state.dx2, e.state.dz2,
            e.state.dxw, e.state.dzw,
            e.state.tx, e.state.tz,
            e.totalReward, e.count, e.avgReward);
    }
    fclose(fp);
}

void LoadQTable(std::vector<QEntry>& qTable) {
    qTable.clear();
    FILE* fp = fopen("ai_qtable.txt", "r");
    if (!fp) return;

    QEntry e;
    while (fscanf(fp, "%d %d %d %d %d %d %d %d %f %d %f",
        &e.state.dx1, &e.state.dz1,
        &e.state.dx2, &e.state.dz2,
        &e.state.dxw, &e.state.dzw,
        &e.state.tx, &e.state.tz,
        &e.totalReward, &e.count, &e.avgReward) == 11)
    {
        qTable.push_back(e);
    }
    fclose(fp);
}

// ai 발사 로직
void AIFireYellowBall(std::vector<QEntry>& qTable) {
    // 현재 게임판 상태
    State baseState = getCurrentState();

    float tx = 0, tz = 0;

    // 1️⃣ 학습된 상태 중 평균보상이 가장 높은 조준을 찾기
    auto best = std::max_element(qTable.begin(), qTable.end(),
        [&](const QEntry& a, const QEntry& b) {
            // 현재 환경이 유사한 상태만 비교
            bool similarA = (abs(a.state.dx1 - baseState.dx1) <= 1 &&
                abs(a.state.dx2 - baseState.dx2) <= 1);
            bool similarB = (abs(b.state.dx1 - baseState.dx1) <= 1 &&
                abs(b.state.dx2 - baseState.dx2) <= 1);
            if (!similarA && !similarB) return false;
            if (similarA && !similarB) return false;
            if (!similarA && similarB) return true;
            return a.avgReward < b.avgReward;
        });

    // 2️⃣ 80% 확률로 best, 20% 확률로 탐색(random)
    if (best != qTable.end() && (rand() % 100) < 80) {
        tx = best->state.tx * 0.5f;  // 다시 실제좌표로 환산
        tz = best->state.tz * 0.5f;
    }
    else {
        tx = ((rand() % 1200) / 100.0f - 6.0f);
        tz = ((rand() % 800) / 100.0f - 4.0f);
    }

    // 파란공 조준점 이동
    blue->setCenter(tx, (float)M_RADIUS, tz);

    // 노란공 발사
    D3DXVECTOR3 target = blue->getCenter();
    D3DXVECTOR3 yellow = gs[2].getCenter();
    double theta = atan2(target.z - yellow.z, target.x - yellow.x);
    double dist = sqrt(pow(target.x - yellow.x, 2) + pow(target.z - yellow.z, 2));
    gs[2].setPower(dist * cos(theta), dist * sin(theta));

    // 현재 상태 저장 (턴 종료 후 보상 업데이트용)
    lastState = baseState;
    lastState.tx = bin(tx - yellow.x);
    lastState.tz = bin(tz - yellow.z);
}

int calculateAIPoint(CSphere& yellowBall) // 보상 점수 계산
{
    bool* h = yellowBall.getHit();  // hit 배열 포인터 반환 (또는 직접 접근 가능)
    int score = 0;

    // 1️⃣ 흰공과 부딪혔으면 무조건 -1점 (파울)
    if (h[3]) {
        score = -1;
    }
    // 2️⃣ 두 빨간공 모두 명중
    else if (h[0] && h[1] && !h[2] && !h[3]) {
        score = +2;
    }
    // 3️⃣ 빨간공 중 하나만 명중
    else if ((h[0] ^ h[1]) && !h[2] && !h[3]) {
        score = +1;
    }
    // 4️⃣ 아무것도 맞추지 못함
    else if (!h[0] && !h[1] && !h[2] && !h[3]) {
        score = -1;
    }

    return score;
}


void OnAITurnEnd() {
    int reward = calculateAIPoint(gs[2]);
    UpdateQTable(QTable, lastState, (float)reward);
    SaveQTable(QTable);

    // 다음 턴 준비: hit 초기화
    bool* hit = gs[2].getHit();
    for (int i = 0; i < 4; i++) hit[i] = false;
}


// -----------------------------------------------------------------------------
// Global variables
// -----------------------------------------------------------------------------
CWall	g_legoPlane;
CWall	g_legowall[4];
CSphere	g_sphere[4];
CSphere	g_target_blueball;
CLight	g_light;

double g_camera_pos[3] = { 0.0, 5.0, -8.0 };

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------


void destroyAllLegoBlock(void)
{
}

// initialization
bool Setup()
{
    int i;

    D3DXMatrixIdentity(&g_mWorld);
    D3DXMatrixIdentity(&g_mView);
    D3DXMatrixIdentity(&g_mProj);

    // create plane and set the position : 바닥 생성, 위치 세팅
    if (false == g_legoPlane.create(Device, -1, -1, 9, 0.03f, 6, d3d::GREEN)) return false;
    g_legoPlane.setPosition(0.0f, -0.0006f / 5, 0.0f);

    // create walls and set the position. note that there are four walls : 벽(4개) 생성,
    if (false == g_legowall[0].create(Device, -1, -1, 9, 0.3f, 0.12f, d3d::DARKRED)) return false;
    g_legowall[0].setPosition(0.0f, 0.12f, 3.06f);
    if (false == g_legowall[1].create(Device, -1, -1, 9, 0.3f, 0.12f, d3d::DARKRED)) return false;
    g_legowall[1].setPosition(0.0f, 0.12f, -3.06f);
    if (false == g_legowall[2].create(Device, -1, -1, 0.12f, 0.3f, 6.24f, d3d::DARKRED)) return false;
    g_legowall[2].setPosition(4.56f, 0.12f, 0.0f);
    if (false == g_legowall[3].create(Device, -1, -1, 0.12f, 0.3f, 6.24f, d3d::DARKRED)) return false;
    g_legowall[3].setPosition(-4.56f, 0.12f, 0.0f);

    // create four balls and set the position : 공(4개 생성)
    for (i = 0; i < 4; i++) {
        if (false == g_sphere[i].create(Device, sphereColor[i])) return false;
        g_sphere[i].setCenter(spherePos[i][0], (float)M_RADIUS, spherePos[i][1]);
        g_sphere[i].setPower(0, 0);
    }

    // create blue ball for set direction
    if (false == g_target_blueball.create(Device, d3d::BLUE)) return false;
    g_target_blueball.setCenter(.0f, (float)M_RADIUS, .0f);

    // light setting 
    D3DLIGHT9 lit;   // 조명 객체 생성?
    ::ZeroMemory(&lit, sizeof(lit));
    lit.Type = D3DLIGHT_POINT;
    lit.Diffuse = d3d::WHITE;
    lit.Specular = d3d::WHITE * 0.9f;
    lit.Ambient = d3d::WHITE * 0.9f;
    //lit.Position = D3DXVECTOR3(0.3f, 4.0f, -4.0f);
    lit.Position = D3DXVECTOR3(0.0f, 3.0f, 0.0f);
    lit.Range = 100.0f;
    lit.Attenuation0 = 0.0f;
    lit.Attenuation1 = 0.9f;
    lit.Attenuation2 = 0.0f;
    /*lit.Type = D3DLIGHT_SPOT;
    lit.Diffuse = D3DXCOLOR(1.0f, 0.95f, 0.8f, 1.0f);
    lit.Specular = D3DXCOLOR(1.0f, 0.9f, 0.7f, 1.0f);
    lit.Ambient = D3DXCOLOR(0.3f, 0.25f, 0.2f, 1.0f);
    lit.Position = D3DXVECTOR3(0.0f, 5.0f, -2.0f);
    lit.Direction = D3DXVECTOR3(0.0f, -1.0f, 0.3f);
    lit.Range = 30.0f;
    lit.Falloff = 1.0f;
    lit.Attenuation0 = 0.5f;
    lit.Attenuation1 = 0.05f;
    lit.Theta = D3DXToRadian(25.0f);
    lit.Phi = D3DXToRadian(40.0f);*/
    if (false == g_light.create(Device, lit))
        return false;

    // --- 점수판용 폰트 생성 (크고 가시성 좋은 폰트) ---
    D3DXFONT_DESC fontDesc = {
        38,                        // Height (글자 크기)
        0,                         // Width (0이면 자동)
        FW_HEAVY,                  // Weight (아주 굵게)
        1,                         // MipLevels
        FALSE,                     // Italic (기울임 없음)
        DEFAULT_CHARSET,
        OUT_TT_ONLY_PRECIS,        // TrueType 폰트 사용
        ANTIALIASED_QUALITY,       // 부드러운 렌더링
        DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI Black"           // 폰트 이름 (두껍고 깔끔한 글꼴)
    };

    if (FAILED(D3DXCreateFontIndirect(Device, &fontDesc, &g_pFont))) {
        return false;
    }


    //// Position and aim the camera. : 카메라 설정
    //D3DXVECTOR3 pos(0.0f, 5.0f, -8.0f);
    //D3DXVECTOR3 target(0.0f, 0.0f, 0.0f);
    //D3DXVECTOR3 up(0.0f, 2.0f, 0.0f);
    //D3DXMatrixLookAtLH(&g_mView, &pos, &target, &up);
    //Device->SetTransform(D3DTS_VIEW, &g_mView);

    //카메라뷰
    D3DXVECTOR3 eye(0.0f, 10.0f, 0.0f);    // 위쪽에서
    D3DXVECTOR3 lookAt(0.0f, 0.0f, 0.0f); //테이블 중심 바라봄
    D3DXVECTOR3 up(0.0f, 0.0f, -1.0f);  //위에서 아래보기
    D3DXMatrixLookAtLH(&g_mView, &eye, &lookAt, &up);
    Device->SetTransform(D3DTS_VIEW, &g_mView);



    // Set the projection matrix. : 투영 행렬 설정
    D3DXMatrixPerspectiveFovLH(&g_mProj, D3DX_PI / 4,
        (float)Width / (float)Height, 1.0f, 100.0f);
    Device->SetTransform(D3DTS_PROJECTION, &g_mProj);

    // Set render states.
    Device->SetRenderState(D3DRS_LIGHTING, TRUE);
    Device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
    Device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);

    g_light.setLight(Device, g_mWorld);
    return true;
}

void Cleanup(void)
{
    g_legoPlane.destroy();
    for (int i = 0; i < 4; i++) {
        g_legowall[i].destroy();
    }
    if (g_pFont) { // 폰트 해제
        g_pFont->Release();
        g_pFont = NULL;
    }
    destroyAllLegoBlock();
    g_light.destroy();

    SaveQTable(QTable);

}

void updateScore(CSphere& ball) {
    int score = ball.getScore();

    switch (isWhiteTurn) {
        // 하얀공 턴
    case (1):
        if (score == 1) {
            whiteScore += 1;
        }
        else if (score == -1) {
            whiteScore += -1;
            isWhiteTurn = -isWhiteTurn; // turn change
        }
        else if (score == 0) {
            isWhiteTurn = -isWhiteTurn; // turn change
        }
        break;
        // 노란공 턴
    case (-1):
        if (score == 1) {
            yellowScore += 1;
        }
        else if (score == -1) {
            yellowScore += -1;
            isWhiteTurn = -isWhiteTurn; // turn change
        }
        else if (score == 0) {
            isWhiteTurn = -isWhiteTurn; // turn change
        }
        break;
    default:
        break;
    }

    for (int i = 0; i < 4; i++) {
        g_sphere[i].hit_initialize();
    }

    if (isWhiteTurn == -1) { // 노란공일떄 학습 업데이트
        OnAITurnEnd();
    }

    // 승리 판별하기.
    if (whiteScore >= winScore) {
        winner = 3;
    }
    else if (yellowScore >= winScore) {
        winner = 2;
    }
    // 판별해서 이긴쪽. 폰트 생성? -> display() 마다 보이도록
    isInitBlue = false;
}

// timeDelta represents the time between the current image frame and the last image frame.
// the distance of moving balls should be "velocity * timeDelta"
bool Display(float timeDelta)   // 매 프레임 실행
{
    int i = 0;
    int j = 0;


    if (Device)
    {
        Device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00afafaf, 1.0f, 0);
        Device->BeginScene();

        // update the position of each ball. during update, check whether each ball hit by walls.
        for (i = 0; i < 4; i++) {
            g_sphere[i].ballUpdate(timeDelta);
            for (j = 0; j < 4; j++) { g_legowall[i].hitBy(g_sphere[j]); }
        }

        // check whether any two balls hit together and update the direction of balls
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                if (i >= j) { continue; }
                g_sphere[i].hitBy(g_sphere[j]);
            }
        }

        // white Turn 일 때 파란공 위치 초기화
        if ((isWhiteTurn == 1) && !isTurnStarted && !isInitBlue) { // 하얀색 공 턴이고 && 턴이 아직 시작 안된 상태고, isInitBlue가 false 일때 (그니까 매 프레임마다 가운데 위치로 셋되면 절대 안되니까, isInitBlue가 false일때만 하는걸로 하고, 턴 중에는 true 유지, 이후에 updateScore()에서 false 로 변함)
            g_target_blueball.setCenter(.0f, (float)M_RADIUS, .0f);
            isInitBlue = true;
        }

        // 모든 공이 거의 멈췄는지 체크
        bool allStopped = true;
        for (i = 0; i < 4; i++) {
            if (fabs(g_sphere[i].getVelocity_X()) > 0.03 ||
                fabs(g_sphere[i].getVelocity_Z()) > 0.03) {
                allStopped = false;
                break;
            }
        }

        // 모든 공이 멈췄으면 점수 계산
        if (allStopped && isTurnStarted) { // isTurnStarted
            if (isWhiteTurn == 1)
                updateScore(g_sphere[3]);  // white
            else
                updateScore(g_sphere[2]);  // yellow
            isTurnStarted = false; // 한 번만 계산되게
            showGuideLine = true;  // 모든 공이 멈추면 조준선 다시 표시
        }

        // draw plane, walls, and spheres
        g_legoPlane.draw(Device, g_mWorld);
        for (i = 0; i < 4; i++) {
            g_legowall[i].draw(Device, g_mWorld);
            g_sphere[i].draw(Device, g_mWorld);
        }
        g_target_blueball.draw(Device, g_mWorld);
        g_light.draw(Device);

        // draw plane, walls, and spheres
        g_legoPlane.draw(Device, g_mWorld);
        for (i = 0; i < 4; i++) {
            g_legowall[i].draw(Device, g_mWorld);
            g_sphere[i].draw(Device, g_mWorld);
        }
        g_target_blueball.draw(Device, g_mWorld);
        g_light.draw(Device);


        // 조준선 (showGuideLine이 true일 때, white공 턴일때만 표시)
        if (showGuideLine && isWhiteTurn == 1)
        {
            D3DXVECTOR3 cueBallPos = g_sphere[3].getCenter();
            D3DXVECTOR3 blueBallPos = g_target_blueball.getCenter();

            float lineY = TABLE_Y + M_RADIUS * 0.2f - 2.7;
            cueBallPos.y = blueBallPos.y = lineY;

            D3DXVECTOR3 dir = blueBallPos - cueBallPos;
            D3DXVec3Normalize(&dir, &dir);

            const float MAX_DIST = 200.0f;
            const float SEG = 0.15f;   // 점선 segment 길이
            const float GAP = 0.15f;   // 점선 간격


            D3DXVECTOR3 p0 = cueBallPos;
            D3DXVECTOR3 dirNow = dir;

            int reflectCount = 0;

            // 렌더링 설정
            Device->SetRenderState(D3DRS_LIGHTING, FALSE);
            Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            Device->SetFVF(D3DFVF_LINEVERTEX);

            D3DCOLOR guideColor = D3DCOLOR_ARGB(200, 255, 200, 0);

            while (reflectCount < 2)
            {
                // 현 위치 p0에서 다음 벽 충돌점 계산
                float hitDist = 9999.0f;
                D3DXVECTOR3 hitPoint;
                bool hitOccurred = false;

                // X 방향 벽
                if (dirNow.x > 0)
                {
                    float tx = (TABLE_X_MAX - p0.x) / dirNow.x;
                    if (tx > 0 && tx < hitDist)
                    {
                        hitDist = tx;
                        hitPoint = p0 + dirNow * tx;
                        hitOccurred = true;
                    }
                }
                else if (dirNow.x < 0)
                {
                    float tx = (TABLE_X_MIN - p0.x) / dirNow.x;
                    if (tx > 0 && tx < hitDist)
                    {
                        hitDist = tx;
                        hitPoint = p0 + dirNow * tx;
                        hitOccurred = true;
                    }
                }

                // Z 방향 벽
                if (dirNow.z > 0)
                {
                    float tz = (TABLE_Z_MAX - p0.z) / dirNow.z;
                    if (tz > 0 && tz < hitDist)
                    {
                        hitDist = tz;
                        hitPoint = p0 + dirNow * tz;
                        hitOccurred = true;
                    }
                }
                else if (dirNow.z < 0)
                {
                    float tz = (TABLE_Z_MIN - p0.z) / dirNow.z;
                    if (tz > 0 && tz < hitDist)
                    {
                        hitDist = tz;
                        hitPoint = p0 + dirNow * tz;
                        hitOccurred = true;
                    }
                }

                if (!hitOccurred) break;

                // p0 >> hitPoint 를 점선으로 그리기, Gap 반복
                float totalDist = hitDist;
                for (float t = 0; t < totalDist; t += (SEG + GAP))
                {
                    float t0 = t;
                    float t1 = t + SEG;
                    if (t1 > totalDist) t1 = totalDist;

                    D3DXVECTOR3 s = p0 + dirNow * t0;
                    D3DXVECTOR3 e = p0 + dirNow * t1;

                    LineVertex lv[2] = {
                        { s, guideColor },
                        { e, guideColor }
                    };
                    Device->DrawPrimitiveUP(D3DPT_LINELIST, 1, lv, sizeof(LineVertex));
                }

                // 반사 처리
                bool reflectX = false, reflectZ = false;

                if (fabs(hitPoint.x - TABLE_X_MIN) < 0.01f || fabs(hitPoint.x - TABLE_X_MAX) < 0.01f)
                    reflectX = true;
                if (fabs(hitPoint.z - TABLE_Z_MIN) < 0.01f || fabs(hitPoint.z - TABLE_Z_MAX) < 0.01f)
                    reflectZ = true;

                if (reflectX) dirNow.x *= -1;
                if (reflectZ) dirNow.z *= -1;

                p0 = hitPoint;

                reflectCount++;
            }

            Device->SetRenderState(D3DRS_LIGHTING, TRUE);
            Device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        }



        // 점수판 및 턴 표시
        if (g_pFont) {
            RECT rectWhite, rectYellow, rectTurn;

            // 점수판 중앙 상단 위치
            SetRect(&rectWhite, Width / 2 - 250, 40, 0, 0);
            SetRect(&rectYellow, Width / 2 + 70, 40, 0, 0);
            SetRect(&rectTurn, Width / 2 - 120, 100, 0, 0); // 턴 표시

            // 점수 문자열
            char whiteText[64], yellowText[64], turnText[64];
            sprintf_s(whiteText, "WHITE: %d", whiteScore);
            sprintf_s(yellowText, "YELLOW: %d", yellowScore);

            // 턴 표시 문자열
            if (isWhiteTurn == 1)
                sprintf_s(turnText, "WHITE TURN !");
            else
                sprintf_s(turnText, "YELLOW TURN ! \n  (Press Space)");

            // 그림자용 사각형 (글자 대비용)
            RECT shadowWhite = rectWhite;
            RECT shadowYellow = rectYellow;
            RECT shadowTurn = rectTurn;
            OffsetRect(&shadowWhite, 2, 2);
            OffsetRect(&shadowYellow, 2, 2);
            OffsetRect(&shadowTurn, 2, 2);

            // --- 그림자 먼저 출력 ---
            g_pFont->DrawTextA(NULL, whiteText, -1, &shadowWhite, DT_NOCLIP, D3DXCOLOR(0, 0, 0, 0.7f));
            g_pFont->DrawTextA(NULL, yellowText, -1, &shadowYellow, DT_NOCLIP, D3DXCOLOR(0, 0, 0, 0.7f));
            g_pFont->DrawTextA(NULL, turnText, -1, &shadowTurn, DT_NOCLIP, D3DXCOLOR(0, 0, 0, 0.7f));

            // --- 본문 텍스트 출력 ---
            g_pFont->DrawTextA(NULL, whiteText, -1, &rectWhite, DT_NOCLIP, D3DXCOLOR(0.4f, 0.8f, 1.0f, 1.0f));   // 밝은 파란색 (WHITE 팀)
            g_pFont->DrawTextA(NULL, yellowText, -1, &rectYellow, DT_NOCLIP, D3DXCOLOR(1.0f, 0.9f, 0.3f, 1.0f)); // 금빛 노란색 (YELLOW 팀)

            // 턴 표시 색상: 현재 턴에 따라 다르게 강조
            if (isWhiteTurn == 1)
                g_pFont->DrawTextA(NULL, turnText, -1, &rectTurn, DT_NOCLIP, D3DXCOLOR(0.5f, 0.8f, 1.0f, 1.0f)); // 하늘색 계열
            else
                g_pFont->DrawTextA(NULL, turnText, -1, &rectTurn, DT_NOCLIP, D3DXCOLOR(1.0f, 0.8f, 0.3f, 1.0f)); // 노란빛
        }


        Device->EndScene();
        Device->Present(0, 0, 0, 0);
        Device->SetTexture(0, NULL);
    }
    return true;
}

LRESULT CALLBACK d3d::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static bool wire = false;   // 와이어프레임 (테두리만 보이는) 모드 전환 여부
    static bool isReset = true;   // 마우스가 새로 클릭되었는 지 여부
    static int old_x = 0;   // 이전 마우스 좌표 (마우스 이동량 계산용)
    static int old_y = 0;
    static enum { WORLD_MOVE, LIGHT_MOVE, BLOCK_MOVE } move = WORLD_MOVE;

    // 디버깅용 로컬 변수
    int score;

    switch (msg) {
    case WM_DESTROY:   // 창 닫힘 처리. (사용자가 창 닫을 때)
    {
        ::PostQuitMessage(0);
        break;
    }
    case WM_KEYDOWN:   // 키보드 입력 처리 (키가 눌릴때마다)
    {
        switch (wParam) {
        case VK_ESCAPE:
            ::DestroyWindow(hwnd);
            break;
        case VK_RETURN:
            if (NULL != Device) {
                wire = !wire;
                Device->SetRenderState(D3DRS_FILLMODE,
                    (wire ? D3DFILL_WIREFRAME : D3DFILL_SOLID));
            }
            break;
        case VK_SPACE:   // 핵심 조작 로직 : 파란공과 흰공 위치 이용해 발사 방향 계산

            showGuideLine = false;   // 선 숨김

            // white, yellow 바꾸기.

            D3DXVECTOR3 targetpos = g_target_blueball.getCenter(); // 이건 if 문에 포함 x
            if (isWhiteTurn == 1) {
                D3DXVECTOR3	whitepos = g_sphere[3].getCenter();
                double theta = acos(sqrt(pow(targetpos.x - whitepos.x, 2)) / sqrt(pow(targetpos.x - whitepos.x, 2) +
                    pow(targetpos.z - whitepos.z, 2)));		// 기본 1 사분면
                if (targetpos.z - whitepos.z <= 0 && targetpos.x - whitepos.x >= 0) { theta = -theta; }	//4 사분면
                if (targetpos.z - whitepos.z >= 0 && targetpos.x - whitepos.x <= 0) { theta = PI - theta; } //2 사분면
                if (targetpos.z - whitepos.z <= 0 && targetpos.x - whitepos.x <= 0) { theta = PI + theta; } // 3 사분면
                double distance = sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.z - whitepos.z, 2));
                g_sphere[3].setPower(distance * cos(theta), distance * sin(theta));
            }
            else if (isWhiteTurn == -1) { // 노란공일 때
                AIFireYellowBall(QTable);
                /*D3DXVECTOR3	yellowpos = g_sphere[2].getCenter();
                double theta = acos(sqrt(pow(targetpos.x - yellowpos.x, 2)) / sqrt(pow(targetpos.x - yellowpos.x, 2) +
                    pow(targetpos.z - yellowpos.z, 2)));		// 기본 1 사분면
                if (targetpos.z - yellowpos.z <= 0 && targetpos.x - yellowpos.x >= 0) { theta = -theta; }	//4 사분면
                if (targetpos.z - yellowpos.z >= 0 && targetpos.x - yellowpos.x <= 0) { theta = PI - theta; } //2 사분면
                if (targetpos.z - yellowpos.z <= 0 && targetpos.x - yellowpos.x <= 0) { theta = PI + theta; } // 3 사분면
                double distance = sqrt(pow(targetpos.x - yellowpos.x, 2) + pow(targetpos.z - yellowpos.z, 2));
                g_sphere[2].setPower(distance * cos(theta), distance * sin(theta));*/
            }
            else {
                // 에러 처리
            }

            // 처음으로 눌렸을때 -> 게임 시작이니까 상태 변환
            isTurnStarted = true;

            // 턴 체인지의 시작점. -> 취소
            // updateScore(); -> 취소


        }
        break;
    }

    //case WM_MOUSEMOVE:   // 마우스 이동 처리 : 마우스의 현재 좌표를 받아옴. 왼쪽/오른쪽 버튼 뭐 눌렸는지에 따라
    //{
    //    int new_x = LOWORD(lParam);
    //    int new_y = HIWORD(lParam);
    //    float dx;
    //    float dy;

    //    if (LOWORD(wParam) & MK_LBUTTON) {

    //        if (isReset) {
    //            isReset = false;
    //        }
    //        else {
    //            D3DXVECTOR3 vDist;
    //            D3DXVECTOR3 vTrans;
    //            D3DXMATRIX mTrans;
    //            D3DXMATRIX mX;
    //            D3DXMATRIX mY;

    //            switch (move) {
    //            case WORLD_MOVE:
    //                dx = (old_x - new_x) * 0.01f;
    //                dy = (old_y - new_y) * 0.01f;
    //                D3DXMatrixRotationY(&mX, dx);
    //                D3DXMatrixRotationX(&mY, dy);
    //                g_mWorld = g_mWorld * mX * mY;

    //                break;
    //            }
    //        }

    //        old_x = new_x;
    //        old_y = new_y;

    //    }
    //    else {
    //        isReset = true;

    //        if (LOWORD(wParam) & MK_RBUTTON) {
    //            dx = (old_x - new_x);// * 0.01f;
    //            dy = (old_y - new_y);// * 0.01f;

    //            D3DXVECTOR3 coord3d = g_target_blueball.getCenter();
    //            g_target_blueball.setCenter(coord3d.x + dx * (-0.007f), coord3d.y, coord3d.z + dy * 0.007f);
    //        }
    //        old_x = new_x;
    //        old_y = new_y;

    //        move = WORLD_MOVE;
    //    }
    //    break;
    //}
    case WM_MOUSEMOVE:
    {
        int new_x = LOWORD(lParam);
        int new_y = HIWORD(lParam);

        // 마우스 왼쪽으로 회전하는 기능 제거
        if (LOWORD(wParam) & MK_RBUTTON) {
            if (isWhiteTurn == 1){
              
              // 파란공 이동만 허용
              float dx = (old_x - new_x);
              float dy = (old_y - new_y);
              D3DXVECTOR3 coord3d = g_target_blueball.getCenter();
              g_target_blueball.setCenter(coord3d.x + dx * (-0.007f), coord3d.y, coord3d.z + dy * 0.007f);
            }
            else; // player 2의 차례일 때에는 입력을 받지 않고 기다림.
        }

        old_x = new_x;
        old_y = new_y;
        break;
    }

    }

    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hinstance,
    HINSTANCE prevInstance,
    PSTR cmdLine,
    int showCmd)
{
    // 디버깅용 콘솔이 필요하시면 주석 해제해 주세요
    /*
    AllocConsole();

    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONIN$", "r", stdin);ㅣ화기

    std::cout << "=== Console Initialized ===" << std::endl;

    // 디버깅용 콘솔 생성 종료
    */
    LoadQTable(QTable);

    srand(static_cast<unsigned int>(time(NULL)));

    gs = g_sphere; // 배열 가리킴.
    blue = &g_target_blueball; // 파란공 가리킴

    if (!d3d::InitD3D(hinstance,   // Direct3D 초기화
        Width, Height, true, D3DDEVTYPE_HAL, &Device))
    {
        ::MessageBox(0, "InitD3D() - FAILED", 0, 0);
        return 0;
    }

    if (!Setup())   // 오브젝트, 조명, 카메라 초기 설정
    {
        ::MessageBox(0, "Setup() - FAILED", 0, 0);
        return 0;
    }

    d3d::EnterMsgLoop(Display);   // Display() 반복 호출 (게임 루프)

    Cleanup();   // 리소스 정리

    Device->Release();

    return 0;
}