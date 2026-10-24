#include "JSystem/JGeometry/Vec.h"
#include "Kaneshige/Course/CrsGround.h"
#include "Kaneshige/RaceMgr.h"
#include "Osako/kartPad.h"
#include "Sato/GeographyObj.h"
#include "Sato/GeographyObjMgr.h"
#include "Sato/ItemObj.h"
#include "Sato/ItemObjMgr.h"
#include "Sato/JPEffectPerformer.h"
#include "Sato/stEffectMgr.h"
#include "Yamamoto/KartGame.h"
#include "Yamamoto/kartBody.h"
#include "Yamamoto/kartCtrl.h"
#include "Yamamoto/kartParams.h"

#include "JSystem/JAudio/JASFakeMatch2.h"
#include "dolphin/mtx.h"
#include "types.h"

bool KartGame::Init(int kartNo) {
    mBody = GetKartCtrl()->getKartBody(kartNo);
    _8 = 0;
    _9 = 0;
    _38.zero();
    mJugemPoint = NULL;
    _18[0] = 0.f;
    _18[1] = 0.f;
    _E = 0;
    _10 = 0;
    mCountDownDuration = 0;

    _A = 0;
    mTimeToChange = 0;
    _C = 0;

    bool isRight = RaceMgr::getCurrentManager()->getStartPoint(&_20, &_2C, kartNo);

    _20.y += 300.f;

    return isRight;
}

void KartGame::GetGorundTireNum() {
    u8 num = mBody->mMynum;

    KartBody *body = mBody;
    body->mTouchNum = 0;

    KartSus *sus[4];
    int idx = num;
    sus[0] = GetKartCtrl()->getKartSus(idx * 4 + 0);
    sus[1] = GetKartCtrl()->getKartSus(idx * 4 + 1);
    sus[2] = GetKartCtrl()->getKartSus(idx * 4 + 2);
    sus[3] = GetKartCtrl()->getKartSus(idx * 4 + 3);

    if (sus[0]->_124 & 1)
        ++body->mTouchNum;

    if (sus[1]->_124 & 1)
        ++body->mTouchNum;

    if (sus[2]->_124 & 1)
        ++body->mTouchNum;

    if (sus[3]->_124 & 1)
        ++body->mTouchNum;

    GetKartCtrl()->getKartSound(num)->DoSlipSound(num);

    // TODO: what kind of vectors are this?
    JGeometry::TVec3f _vec0;
    _vec0.set(body->mPlayerPosMtx[0][3], body->mPlayerPosMtx[1][3], body->mPlayerPosMtx[2][3]);

    body->mShadowArea.searchShadow(_vec0);

    if (body->mTouchNum != 0 && body->_58c == 7) {
        // TODO: is getObject return type wrong or is the cast here ok?
        // first one probably
        ExGeographyObj *exGeo = static_cast<ExGeographyObj *>(body->mBodyGround.getObject());

        JGeometry::TVec3f _vec2;
        _vec2.set(0.f, -3.5f, 0.f);

        exGeo->AddVel(_vec0, _vec2);
    }
}

void KartGame::WatchEffectAcceleration() {
    KartBody *body = mBody;
    int num = body->mMynum;

    const KartGamePad *gamePad = GetKartCtrl()->GetDriveCont(num);
    const KartPad *pad = GetKartCtrl()->getKartPad(num);

    if (gamePad->testButton(pad->mAccelBtn)) {
        body->mKartRPM = GetKartCtrl()->fcnvge(body->mKartRPM, 1.f, 0.050000001f, 0.050000001f);
    } else {
        body->mKartRPM = GetKartCtrl()->fcnvge(body->mKartRPM, 0.f, 0.050000001f, 0.050000001f);
    }
}

void KartGame::WatchAcceleration() {
    KartBody *body = mBody;
    int num = body->mMynum;

    const KartGamePad *gamePad = GetKartCtrl()->GetDriveCont(num);
    if ((body->mCarStatus & 0x400000) && (body->getRescue()->mState >= 3)) {
        KartPad *pad = GetKartCtrl()->getKartPad(num);
        if (gamePad->testButton(pad->mAccelBtn)) {
            body->_3c8 = GetKartCtrl()->fcnvge(body->_3c8, body->_3d0, 1.f, 1.f);
            _8 |= 2;
        } else {
            GetKartCtrl()->ChaseFnumber(&body->_3c8, 0.f, 1.f);
            _8 &= ~2;
        }
    }
}

void KartGame::DoItmCancel() {
    KartBody *body = mBody;

    body->mCarStatus |= 0x80000000;
    GetItemObjMgr()->abortItemShuffle(body->mMynum);
}

void KartGame::DoStopItm() {
    KartBody *body = mBody;
    u8 num = body->mMynum;

    body->mCarStatus |= 0x80000000;
    GetItemObjMgr()->abortItemShuffle(body->mMynum);

    ItemObjMgr *itemMgr = GetItemObjMgr();
    ItemObj *item = itemMgr->getKartEquipItem(num, 0);
    itemMgr->deleteHeartItem(num);

    if (item != nullptr) {
        item->IsSuccessionItem() ? item->setChildStateForceDisappear()
                                 : item->setStateForceDisappear();
    }

    item = itemMgr->getKartEquipItem(num, 1);
    if (item != nullptr) {
        item->IsSuccessionItem() ? item->setChildStateForceDisappear()
                                 : item->setStateForceDisappear();
    }
}

void KartGame::DoChange() {
    KartBody *body = mBody;
    int num = body->mMynum;

    const KartGamePad *gpDriver = GetKartCtrl()->GetDriveCont(num);
    const KartGamePad *gpCoDriv = GetKartCtrl()->GetCoDriveCont(num);

    bool change = false;
    const bool possible = GetKartCtrl()->MakeChangePossible(num);

    if (body->getChecker()->CheckCheange(num)) {
        mTimeToChange = 0;
        return;
    }

    if (mTimeToChange)
        mTimeToChange--;

    // single player kart
    if ((body->mGameStatus & KartBody::HasCoDriver) == false) {
        if (gpDriver->testTrigger(GetKartCtrl()->getKartPad(num)->mTrigZ))
            mTimeToChange = 20;

        if (body->mCarStatus & KartBody::InDriverChange) {
            if (!(body->mExModels[0])->IsChange() && !(body->mExModels[1])->IsChange())
                body->mCarStatus &= ~body->InDriverChange;

        } else if ((gpDriver->testTrigger(GetKartCtrl()->getKartPad(num)->mTrigZ) || possible ||
                    mTimeToChange != 0)) {
            change = true;
            mTimeToChange = 0;
        }
        // with co-driver, but already in change
    } else if (body->mCarStatus & KartBody::InDriverChange) {
        if (gpDriver->testButton(GetKartCtrl()->getKartPad(num)->mTrigZ) &&
            gpCoDriv->testButton(GetKartCtrl()->getKartPad(num)->mTrigZ)) {
            mTimeToChange = 20;
        }

        if (!(body->mExModels[0])->IsChange() && !(body->mExModels[1])->IsChange()) {
            body->mCarStatus &= ~body->InDriverChange;
        }
        // with co-driver and ready to change
    } else if ((gpDriver->testButton(GetKartCtrl()->getKartPad(num)->mTrigZ) &&
                gpCoDriv->testButton(GetKartCtrl()->getKartPad(num)->mTrigZ)) ||
               mTimeToChange != 0) {
        change = true;
        mTimeToChange = 0;
    }

    if (!change)
        return;

    body->mCarStatus &= ~(1ull << 38);
    body->mCarStatus |= body->InDriverChange;

    GetKartCtrl()->getKartAnime(num)->mFlags |= 1;
    GetKartCtrl()->getKartSound(num)->DoChangeVoice();
    GetKartCtrl()->getKartSound(num)->DoChangeStarSound();
}

void KartGame::DoSlide() {
    const int num = mBody->mMynum;
    KartBody *body = mBody;

    if (body->getChecker()->CheckPartsClearKey(num) ||
        !(body->mGameStatus & KartBody::HasCoDriver)) {
        return;
    }

    if (body->mSlideTimer != 0) {
        body->mSlideTimer--;
    }

    if (body->mSlideTimer == 0) {
        body->mCarStatus &= ~body->DoesSlide;
    }

    if ((body->mCarStatus & KartBody::DoesSlide)) {
        if (body->mSlideTimer >= 29) {
            body->_2cc.x += body->_2f0.x * body->_528;
            body->_2cc.y += body->_2f0.y * body->_528;
            body->_2cc.z += body->_2f0.z * body->_528;
        }

        return;
    }

    KartGamePad *gpCoDriv = GetKartCtrl()->GetCoDriveCont(num);
    if (body->getTouchNum() == 0 || (GetKartCtrl()->GetCarSpeed(num) <= 50.f)) {
        return;
    }

    if (!gpCoDriv->testTrigger(JUTGamePad::L) && !gpCoDriv->testTrigger(JUTGamePad::R)) {
        return;
    }

    body->mSlideTimer = 35;

    if (RaceMgr::getCurrentManager()->isMirror()) {
        if (gpCoDriv->testTrigger(JUTGamePad::L)) {
            body->_528 = -50.f * body->_3a4;
            GetKartCtrl()->getKartAnime(num)->mFlags |= 0x10;
            JPEffectPerformer::setEffect(JPEffectPerformer::Effect_Unknown1b, num, body->mPos, 1);
        } else if (gpCoDriv->testTrigger(JUTGamePad::R)) {
            body->_528 = 50.f * body->_3a4;
            GetKartCtrl()->getKartAnime(num)->mFlags |= 0x20;
            JPEffectPerformer::setEffect(JPEffectPerformer::Effect_Unknown1b, num, body->mPos, 0);
        }
    } else {
        if (gpCoDriv->testTrigger(JUTGamePad::R)) {
            body->_528 = -50.f * body->_3a4;
            GetKartCtrl()->getKartAnime(num)->mFlags |= 0x10;
            JPEffectPerformer::setEffect(JPEffectPerformer::Effect_Unknown1b, num, body->mPos, 1);
        } else if (gpCoDriv->testTrigger(JUTGamePad::L)) {
            body->_528 = 50.f * body->_3a4;
            GetKartCtrl()->getKartAnime(num)->mFlags |= 0x20;
            JPEffectPerformer::setEffect(JPEffectPerformer::Effect_Unknown1b, num, body->mPos, 0);
        }
    }

    body->_2cc.x += body->_2f0.x * body->_528;
    body->_2cc.y += body->_2f0.y * body->_528;
    body->_2cc.z += body->_2f0.z * body->_528;

    body->mCarStatus |= body->DoesSlide;
    GetKartCtrl()->getKartSound(num)->DoStrikeSound();
    GetKartCtrl()->getKartSound(num)->DoTandemVoice(2);
}

void KartGame::DoDriftTurboSterr() {
    KartBody *body = mBody;

    if (body->mDriftSterr != 0 && body->mDriftSterr < 0x1E)
        body->mDriftSterr++;
}

void KartGame::SetDriftTurboSterr() {
    KartBody *body = mBody;
    const int num = mBody->mMynum;

    const int threshold = (body->mGameStatus & KartBody::HasCoDriver) ? 2 : 6;
    if ((body->mDriftSterr) < threshold)
        return;

    body->mMTState++;

    if (body->mMTState == 1)
        GetKartCtrl()->getKartSound(num)->DoKartsetSeSound(0x100C9);
    else if (body->mMTState == 2)
        GetKartCtrl()->getKartSound(num)->DoKartsetSeSound(0x100CA);

    if (body->mMTState >= 2)
        body->mMTState = 2;

    body->mDriftSterr = 0;
}

void KartGame::CheckDriftTurbo(void) {
    KartBody *body = mBody;
    int num = body->mMynum;

    if (GetKartCtrl()->GetCarSpeed(num) <= 50.f) {
        body->mCarStatus &= ~(1ull << 41);
        body->mMTBoost = 0;
        body->mDriftSterr = 0;
        body->mMTState = 0;
        return;
    }

    if (body->mGameStatus & KartBody::GsUnknown3)
        return;

    bool didDriftSterr = false;
    KartGamePad *gpCoDriv = GetKartCtrl()->GetCoDriveCont(num);

    if (body->mCarStatus & KartBody::DriftLeft) {
        if (body->mGameStatus & KartBody::HasCoDriver) {
            if (gpCoDriv->getMainStickX() < 0.5f) {
                didDriftSterr = true;
                DoDriftTurboSterr();
            }
        } else if (body->mFrame >= -0.5f) {
            didDriftSterr = true;
            DoDriftTurboSterr();
        }

        if (!didDriftSterr) {
            SetDriftTurboSterr();
            body->mDriftSterr = 1;
        }
    } else {
        if ((body->mCarStatus & KartBody::DriftRight) != 0) {
            if (body->mGameStatus & KartBody::HasCoDriver) {
                if (gpCoDriv->getMainStickX() > -0.3f) {
                    didDriftSterr = true;
                    DoDriftTurboSterr();
                }
            } else {
                if (body->mFrame <= 0.5f) {
                    didDriftSterr = true;
                    DoDriftTurboSterr();
                }
            }

            if (!didDriftSterr) {
                SetDriftTurboSterr();
                body->mDriftSterr = 1;
            }
        } else {
            body->mMTState = 0;
            body->mDriftSterr = 0;
        }
    }

    body->mCarStatus &= ~(1ull << 41);
    body->mMTBoost = 0;
}

f32 KartGame::DoWarmUpRoll() {
    int kartNo = mBody->mMynum;
    KartBody *body = mBody;

    KartGamePad *gpDriver = GetKartCtrl()->GetDriveCont(kartNo);
    if (body->mCarStatus & KartBody::DoesSlide) {
        body->mCarStatus &=
            ~(body->DriftLeft | body->DriftRight | 1ull << 41 | 1ull << 47 | 1ull << 48);
        body->_4d8 = 0.f;
        body->_4d4 = 0.f;
        body->_4c4 = 0.f;
        body->_394 = 0.f;
        body->mLiftframe = 0.f;
        body->_39c = 0.f;
        body->_398 = 0.f;
        body->mMTBoost = 0;
        body->mDriftSterr = 0;
        body->mMTState = 0;
        body->mSterrNorm = 0.f;
    }

    if (body->mCarStatus & (body->DriftLeft | body->DriftRight) &&
        gpDriver->testTrigger(GetKartCtrl()->getKartPad(kartNo)->mTrigL |
                              GetKartCtrl()->getKartPad(kartNo)->mTrigR)) {
        DoDriftClear();
        return 0.f;
    }

    f32 ret;
    if (gpDriver->testButton(GetKartCtrl()->getKartPad(kartNo)->mTrigL |
                             GetKartCtrl()->getKartPad(kartNo)->mTrigR)) {
        if (body->mCarStatus & KartBody::DriftLeft) {
            body->mCarStatus |= (body->DriftLeft | 1ull << 47);
            body->mCarStatus &= ~(body->DriftRight | 1ull << 48);
            ret = 150.f;
            CheckDriftTurbo();
        } else if (body->mCarStatus & KartBody::DriftRight) {
            body->mCarStatus |= (body->DriftRight | 1ull << 48);
            body->mCarStatus &= ~(body->DriftLeft | 1ull << 47);
            ret = -150.f;
            CheckDriftTurbo();
        } else {
            ret = 0.f;
            if (body->mFrame < -0.8f) {
                ret = 150.f;
                body->_4d4 = 0.f;
                body->mCarStatus |= (body->DriftLeft | 1ull << 47);
                body->mCarStatus &= ~(body->DriftRight | 1ull << 48);
                body->mSterrNorm = 0.043611001f;
                if (body->mFrame > -0.90000004f) {
                    body->mSterrNorm = 0.1133888f;
                }
            } else if (body->mFrame > 0.8f) {
                ret = -150.f;
                body->_4d4 = 0.f;
                body->mCarStatus |= (body->DriftRight | 1ull << 48);
                body->mCarStatus &= ~(body->DriftLeft | 1ull << 47);
                body->mSterrNorm = 0.043611001f;
                if (body->mFrame < 0.90000004f) {
                    body->mSterrNorm = 0.1133888f;
                }
            } else {
                body->mMTBoost = 0;
                body->mDriftSterr = 0;
                body->mMTState = 0;
                body->mCarStatus &= ~(1ull << 41);
                body->mCarStatus &=
                    ~(body->DriftLeft | body->DriftRight | 1ull << 47 | 1ull << 48);
            }
        }

        KartAnime *anime = GetKartCtrl()->getKartAnime(kartNo);
        if (!anime->IsProhibition(kartNo) && !body->mDriverModels[0]->IsSit() &&
            !body->getChecker()->CheckOnlyTandemPartsClearKey(kartNo) &&
            !(body->mCarStatus & (body->DriftLeft | body->DriftRight))) {
            GetKartCtrl()->getKartAnime(kartNo)->mFlags |= 0x200u;
        }
    } else {
        if (!(body->mCarStatus & KartBody::CsUnknown15) && body->mDriverModels[0]->IsSit() &&
            !body->mDriverModels[0]->IsStand() &&
            !body->getChecker()->CheckOnlyTandemPartsClearKey(kartNo)) {
            GetKartCtrl()->getKartAnime(kartNo)->mFlags |= 0x400u;
        }

        if (body->mMTState >= 2u) {
            body->mCarStatus |= 1ull << 41;
            body->mMTBoost = body->mMTBoostMax;
            JPEffectPerformer::setEffect(JPEffectPerformer::Effect_Unknown1A, kartNo, body->mPos,
                                         0);
            GetKartCtrl()->getKartSound(body->mMynum)->DoKartMiniTuroboSound();
            GetKartCtrl()->getKartSound(body->mMynum)->DoTandemVoice(0);
            body->getStrat()->DoMotor(MotorManager::MotorType_6);
        }

        body->mDriftSterr = 0;
        body->mMTState = 0;
        body->mCarStatus &=
            ~(body->DriftLeft | body->DriftRight | 1ull << 47 | 1ull << 48);
        ret = 0.f;
    }

    return ret;
}

void KartGame::DoRollAnim() {
    KartBody *body = mBody;
    int kartNo = body->mMynum;

    KartGamePad *gpDriver = GetKartCtrl()->GetDriveCont(kartNo);
    if (gpDriver->getMainStickX() >= 0.2f) {
        body->_394 = -1.0f;
    } else if (gpDriver->getMainStickX() <= -0.2f) {
        body->_394 = 1.0f;
    } else {
        body->_394 = .0f;
    }

    body->mLiftframe =
        GetKartCtrl()->fcnvge(body->mLiftframe, body->_394, 0.050000001f, 0.050000001f);

    if (GetKartCtrl()->getKartAnime(kartNo)->IsDrift(kartNo) == false &&
        GetKartCtrl()->getKartAnime(kartNo)->IsDriftStart(kartNo) == false) {

        if (gpDriver->getMainStickX() >= 0.2f) {
            body->_39c = -1.0f;
        } else if (gpDriver->getMainStickX() <= -0.2f) {
            body->_39c = 1.0f;
        } else {
            body->_39c = 0.0f;
        }

        body->_398 = GetKartCtrl()->fcnvge(body->_398, body->_39c, 0.050000001f, 0.050000001f);
    }

    if (body->mCarStatus & KartBody::DriftLeft) {
        body->_394 = -1.0f;
        body->mLiftframe = -1.0f;
        if (GetKartCtrl()->getKartAnime(kartNo)->IsDriftLeft(kartNo)) {
            body->_39c = 1.0f;
            body->_398 = 1.0f;
        } else {
            body->_39c = -1.0f;
            body->_398 = -1.0f;
        }

        return;
    }

    if (body->mCarStatus & KartBody::DriftRight) {
        body->_394 = 1.0f;
        body->mLiftframe = 1.0f;
        if (GetKartCtrl()->getKartAnime(kartNo)->IsDriftRight(kartNo)) {
            body->_39c = -1.0f;
            body->_398 = -1.0f;
        } else {
            body->_39c = 1.0f;
            body->_398 = 1.0f;
        }
    }
}

void KartGame::DoDriftClear() {
    KartBody *body = mBody;

    body->mMTBoost = 0;
    body->mDriftSterr = 0;
    body->mMTState = 0;
    body->mCarStatus &= ~(1ull << 41);
    body->mCarStatus &=
        ~(body->DriftLeft | body->DriftRight | (1ull << 47) | (1ull << 48));
}

void KartGame::DoRoll() {
    KartBody *body = mBody;

    MakeJump();

    f32 warmupRollRet = DoWarmUpRoll();

    if (GetKartCtrl()->GetCarSpeed(body->mMynum) <= 30.f ||
        GetKartCtrl()->GetCarSpeed(body->mMynum) <= 40.f && body->_3c8 == 0.f) {
        DoDriftClear();
    }

    if (GetKartCtrl()->GetCarSpeed(body->mMynum) < 40.f && body->_510 > 2.44222f) {
        DoDriftClear();
    }

    if (GetKartCtrl()->GetCarSpeed(body->mMynum) <= 80.f && body->_3cc != 0.f) {
        DoDriftClear();
    }

    if (body->mCarStatus & KartBody::CsUnknown3) {
        DoDriftClear();
    }

    if (body->_458 < 2.f) {
        body->_4c4 = GetKartCtrl()->fcnvge(body->_4c4, warmupRollRet * 0.0088235298f, 0.1f, 0.1f);
    } else {
        if (body->_458 < 18.f) {
            body->_4c4 =
                GetKartCtrl()->fcnvge(body->_4c4, warmupRollRet * 0.0023529413f, 0.1f, 0.1f);
        } else {
            int touchNum = body->getTouchNum();
            f32 _unk2 = (touchNum == 0) ? 0.023529412f * warmupRollRet
                                        : (body->_4f8 / 170.f) * warmupRollRet;

            GetKartCtrl()->ChaseFnumber(&body->_4c4, _unk2, body->_3b4);
        }
    }

    if (!(body->mCarStatus & (body->DriftLeft | body->DriftRight))) {
        body->_4c4 = 0.f;
    }

    DoRollAnim();
}

void KartGame::DoTestPitch() {
    KartBody *body = mBody;

    KartGamePad *pad = GetKartCtrl()->GetDriveCont(body->mMynum);
    if (body->getTouchNum() != 0) {
        body->_4c0 = 0.f;
        if (body->mCarStatus & (body->CsUnknown8 | body->CsUnknown9)) {
            body->getStrat()->PitchClear();
        }
        return;
    }

    f32 mainY = pad->getMainStickY();
    if (mainY > 0.f) {
        body->mCarStatus |= body->CsUnknown9;
        body->mCarStatus &= ~body->CsUnknown8;
    } else if (mainY < 0.f) {
        body->mCarStatus |= body->CsUnknown8;
        body->mCarStatus &= ~body->CsUnknown9;
    }

    f32 x;
    f32 y = 0.0;
    if (mainY < 0.5 && mainY > -0.5f) {
        x = 0.0;
    } else {
        y = 0.2f;
        x = 1.5f * mainY;
    }

    GetKartCtrl()->ChaseFnumber(&body->_4c0, x, y);
    if (body->_4c0 == 0.f) {
        body->getStrat()->PitchClear();
    }
}

void KartGame::DoLiftTurbo() {
    KartBody *kartBody = mBody;
    f32 fVar3;
    JGeometry::TVec3f vec3;
    JGeometry::TVec3f vec2;
    JGeometry::TVec3f vec1;
    JGeometry::TVec3f vec0;

    if (!(kartBody->mCarStatus & (KartBody::DriftLeft | KartBody::DriftRight))) {
        kartBody->_518 = 0.0f;
    }

    if (!(kartBody->mCarStatus & (KartBody::DriftLeft | KartBody::DriftRight)) ||
        kartBody->getTouchNum() <= 1 ||
        kartBody->mCarStatus & (KartBody::CsUnknown5 | KartBody::CsUnknown10 | KartBody::CsUnknown26 | KartBody::CsUnknown27) ||
        kartBody->mBodyGround.getAttribute() == 6
    ) {
        kartBody->_564 = 0.0f;
        kartBody->_518 = 0.0f;
        return;
    }

    if (GetKartCtrl()->GetCarSpeed(kartBody->mMynum) <= 30.0f) {
        return;
    }

    if (GetKartCtrl()->GetCarSpeed(kartBody->mMynum) >= 40.0f && kartBody->_3c8 != 0.0f) {
        GetKartCtrl()->DevMatrixByVector(&vec2, &kartBody->mVel, kartBody->_110);
        vec2.x *= 1.075f;
        vec2.z *= 1.015f;

        if (vec2.z < 0.0f) {
            vec2.z *= -1.0f;
        }

        PSMTXMultVecSR(kartBody->_110, &vec2, &kartBody->mVel);
    }

    kartBody->_518 = kartBody->_514;

    f32 fVar2 = kartBody->_4dc * kartBody->_3c8 * kartBody->_518;

    kartBody->_564 = 0.38f;

    f32 fVar4 = kartBody->_564;
    vec3.set(kartBody->_344.x,kartBody->_344.y, kartBody->_344.z);

    fVar3 = fVar2 * fVar4;
    fVar4 = 1.0f - fVar4;
    vec2.set(kartBody->_3a4 * fVar3, 0.0f, kartBody->_3a4 * (fVar2 * fVar4));

    if ((kartBody->mCarStatus & 2) != 0) {
        vec2.x *= -1.0f;
    }

    PSMTXMultVec(kartBody->_110, &vec3, &vec1);
    PSMTXMultVecSR(kartBody->_110, &vec2, &vec0);
    kartBody->DoForce(&vec1, &vec0);

    vec3.set(kartBody->_344.x, kartBody->_344.y,-(kartBody->_344).z * 0.5f);
    vec2.set(kartBody->_3a4 * fVar3, 0.0f, kartBody->_3a4 * (kartBody->_4dc * fVar4));

    if ((kartBody->mCarStatus & 2) != 0) {
        vec2.x *= -1.0f;
    }

    PSMTXMultVec(kartBody->_110, &vec3, &vec1);
    PSMTXMultVecSR(kartBody->_110, &vec2, &vec0);
    kartBody->DoForce(&vec1, &vec0);
}

void KartGame::DoTurbo() {}

void KartGame::DoRollThrow() {
    KartBody *body = mBody;

    JGeometry::TVec3f vec0;
    JGeometry::TVec3f vec1;
    JGeometry::TVec3f vec2;
    JGeometry::TVec3f vec3;
    JGeometry::TVec3f vec4;

    f32 v3 = (3.f * body->_3a4);

    vec0.set(0.f, 70.f, 0.f);
    PSMTXMultVec(body->_110, &vec0, &vec1);

    vec0.set(0.f, 70.f, 100.f);
    PSMTXMultVec(body->_110, &vec0, &vec2);

    vec3.x = vec1.x - vec2.x;
    vec3.y = vec1.y - vec2.y;
    vec3.z = vec1.z - vec2.z;

    f32 vec3LenSqrt = GetKartCtrl()->VectorLengthSqrtf(&vec3);
    if ((vec3LenSqrt - 1.f) > 0.f) {
        vec4.x = vec3.x * (-v3 * (vec3LenSqrt - 1.f) / vec3LenSqrt);
        vec4.y = vec3.y * (-v3 * (vec3LenSqrt - 1.f) / vec3LenSqrt);
        vec4.z = vec3.z * (-v3 * (vec3LenSqrt - 1.f) / vec3LenSqrt);

        body->DoForce(&vec1, &vec4);
    }
}

// literally a no-op always returning 0
int KartGame::DoRollOver() {
    KartBody *body = mBody;

    if (body->getTouchNum() == 0 ||
        body->mCarStatus & (body->CsUnknown12 | body->CsUnknown18) ||
        body->mGameStatus & (body->GsUnknown3)) {
        return 0;
    }

    return 0;
}

void KartGame::DoWanWan() {
    // void ItemWanWanObj::getDifVel() const {}
    // void ItemWanWanObj::getPullVec(JGeometry::TVec3<float> *) {}
}

bool KartGame::DoPushStart() {
    const int timerEnd = 30;
    KartBody *body = mBody;

    body->_594++;
    body->_3c8 = body->_3d0;
    if (body->_594 == timerEnd) {
        body->mCarStatus &= ~(body->CsUnknown25);
        return true;
    }

    return false;
}

void KartGame::DoBalance(f32 *a2, f32 a3) {
    KartBody *kartBody = mBody;

    if ((mBody->mCarStatus & (KartBody::CsUnknown5 | KartBody::CsUnknown10
                             | KartBody::CsUnknown26 | KartBody::CsUnknown27))) {
        return;
    }

    f32 v0 = kartBody->_468;
    if (v0 > -0.40122193f && v0 < 0.40122193f) {
        return;
    }

    *a2 *= a3;
}

void KartGame::MakeClear() {
    KartBody *body = mBody;

    body->_3c8 = 0.f;
    body->getStrat()->LiftClear();
    body->getStrat()->PitchClear();
    body->getStrat()->AllGravyClear();
    body->getStrat()->DashClear();
    body->getStrat()->OtherClear();
}

void KartGame::MakeBoardDash() {
    const int boostTime = 60;
    int kartNo = mBody->mMynum;
    KartBody *body = mBody;

    if (body->mCarStatus & (KartBody::CsUnknown12)) {
        return;
    }

    GetKartCtrl()->getKartSound(kartNo)->DoDashSound();

    body->mCarStatus &= ~(body->CsUnknown14 | body->CsUnknown29 | (1ull << 42));

    body->getStrat()->DoMotor(MotorManager::MotorType_7);

    if (body->mCarStatus & KartBody::CsUnknown17) {
        body->mBoostTimer = boostTime;
        return;
    }

    body->mCarStatus |= (body->CsUnknown15 | body->CsUnknown17);
    body->mBoostTimer = boostTime;

    body->_52c = 0.4f;
    body->_474 = 0.313f;

    JPEffectPerformer::setEffect(JPEffectPerformer::Effect_Unknown17, kartNo, body->mPos, 2);

    if (!GetKartCtrl()->CheckCamera(kartNo)) {
        return;
    }

    int camNo = GetKartCtrl()->GetCameraNum(kartNo);
    KartCam *cam = GetKartCtrl()->getKartCam(camNo);
    if (cam->GetCameraMode()) {
        return;
    }

    JPEffectPerformer::setEffectEachCam(JPEffectPerformer::Effect_Unknown23, kartNo, camNo, 0);
}

void KartGame::MakeJumpDash() {
    if ((mBody->mCarStatus & KartBody::CsUnknown12)) {
        return;
    }

    KartBody *body = mBody;

    int kartNo = body->mMynum;
    GetKartCtrl()->getKartSound(kartNo)->DoDashSound();
    body->mCarStatus &= ~(body->CsUnknown14 | body->CsUnknown16 | body->CsUnknown17 | (1ull << 42));
    body->getStrat()->DoMotor(MotorManager::MotorType_7);

    if (body->mCarStatus & KartBody::CsUnknown29) {
        body->mBoostTimer = 20;
        return;
    }

    body->mCarStatus |= body->CsUnknown15 | body->CsUnknown29;
    body->mBoostTimer = 20;
    body->_52c = 0.4f;
    body->_474 = 0.313f;

    JPEffectPerformer::setEffect(JPEffectPerformer::Effect_Unknown17, kartNo, body->mPos, 2);

    if (GetKartCtrl()->CheckCamera(kartNo)) {
        int camNo = GetKartCtrl()->GetCameraNum(kartNo);
        KartCam *cam = GetKartCtrl()->getKartCam(camNo);

        if (cam->GetCameraMode() == 0) {
            JPEffectPerformer::setEffectEachCam(JPEffectPerformer::Effect_Unknown23, kartNo, camNo, 0);
        }
    }

    mJugemPoint = body->mBodyGround.getJugemPoint();
    if (mJugemPoint) {
        JGeometry::TVec3f(); // ?????
        mJugemPoint->getPosition(&_38);
        return;
    }

    _18[1] = 0.f;
    _18[0] = 0.f;
    mJugemPoint = NULL; // it's already null lmao
    _38.set(body->mPos);
}

// basically MakeJumpDash, but here they swapped CsUnknown29 and CsUnknown42 (mCarStatus)
void KartGame::MakeSpJumpDash() {
    int kartNo = mBody->mMynum;
    KartBody *body = mBody;

    if (mBody->mCarStatus & KartBody::CsUnknown12) {
        return;
    }

    GetKartCtrl()->getKartSound(kartNo)->DoDashSound();
    // toggles 29 off instead of 42
    body->mCarStatus &= ~(body->CsUnknown14 | body->CsUnknown16 | body->CsUnknown17 | body->CsUnknown29);
    body->getStrat()->DoMotor(MotorManager::MotorType_7);

    // checks 42 (MakeJumpDash checks 29 here)
    if (body->mCarStatus & (1ull << 42)) {
        body->mBoostTimer = 20;
        return;
    }

    // sets 42 instead of 29 like MakeJumpDash
    body->mCarStatus |= body->CsUnknown15 | (1ull << 42);
    body->mBoostTimer = 20;
    body->_52c = 0.4f;
    body->_474 = 0.313f;

    JPEffectPerformer::setEffect(JPEffectPerformer::Effect_Unknown17, kartNo, body->mPos, 2);

    if (GetKartCtrl()->CheckCamera(kartNo)) {
        int camNo = GetKartCtrl()->GetCameraNum(kartNo);
        KartCam *cam = GetKartCtrl()->getKartCam(camNo);

        if (cam->GetCameraMode() == 0) {
            JPEffectPerformer::setEffectEachCam(JPEffectPerformer::Effect_Unknown23, kartNo, camNo, 0);
        }
    }

    mJugemPoint = body->mBodyGround.getJugemPoint();
    if (mJugemPoint) {
        JGeometry::TVec3f();
        mJugemPoint->getPosition(&_38);
        return;
    }

    _18[1] = 0.f;
    _18[0] = 0.f;
    mJugemPoint = NULL;
    _38.set(body->mPos);
}

void KartGame::MakeMashDash() {
    int kartNo = mBody->mMynum;
    KartBody *body = mBody;

    if (mBody->mCarStatus & KartBody::CsUnknown12) {
        return;
    }

    GetStEfctMgr()->createKinokoDashEmt(kartNo);
    GetKartCtrl()->getKartSound(kartNo)->DoMashDashSound();
    GetKartCtrl()->getKartSound(kartNo)->DoDashVoice();

    if (body->mCarStatus & (body->CsUnknown17 | body->CsUnknown29 | (1ull << 42))) {
        return;
    }

    body->mCarStatus &= ~(body->CsUnknown16 | body->CsUnknown17 | body->CsUnknown29 | (1ull << 42));
    body->getStrat()->DoMotor(MotorManager::MotorType_5);

    if (body->mCarStatus & KartBody::CsUnknown14) {
        body->mBoostTimer = 80;
        return;
    }

    body->mCarStatus |= body->CsUnknown14 | body->CsUnknown15;
    body->mBoostTimer = 80;
    body->_52c = 0.4f;
    body->_474 = 0.313f;

    if (GetKartCtrl()->CheckCamera(kartNo)) {
        int camNo = GetKartCtrl()->GetCameraNum(kartNo);
        KartCam *cam = GetKartCtrl()->getKartCam(camNo);

        if (cam->GetCameraMode() == 0) {
            JPEffectPerformer::setEffectEachCam(JPEffectPerformer::Effect_Unknown23, kartNo, camNo, 0);
        }
    }
}

void KartGame::MakeGoldenMashDash() {
    int kartNo = mBody->mMynum;
    KartBody *body = mBody;

    if (mBody->mCarStatus & KartBody::CsUnknown12) {
        return;
    }

    GetStEfctMgr()->createKinokoDashEmt(kartNo);
    GetKartCtrl()->getKartSound(kartNo)->DoMashDashSound();

    if (body->mCarStatus & (body->CsUnknown17 | body->CsUnknown29 | (1ull << 42))) {
        return;
    }

    body->mCarStatus &= ~(body->CsUnknown17 | body->CsUnknown29 | (1ull << 42));
    body->getStrat()->DoMotor(MotorManager::MotorType_5);

    if (body->mCarStatus & KartBody::CsUnknown14) {
        body->mBoostTimer = 80;
        GetKartCtrl()->getKartSound(kartNo)->DoGoldenDashVoice();
        return;
    }

    GetKartCtrl()->getKartSound(kartNo)->DoGoldenDashVoice();
    body->mCarStatus |= body->CsUnknown14 | body->CsUnknown15;
    body->mBoostTimer = 80;
    body->_52c = 0.4f;
    body->_474 = 0.313f;

    if (GetKartCtrl()->CheckCamera(kartNo)) {
        int camNo = GetKartCtrl()->GetCameraNum(kartNo);
        KartCam *cam = GetKartCtrl()->getKartCam(camNo);

        if (cam->GetCameraMode() == 0) {
            JPEffectPerformer::setEffectEachCam(JPEffectPerformer::Effect_Unknown23, kartNo, camNo, 0);
        }
    }
}

void KartGame::MakeStartDash() {
    int kartNo = mBody->mMynum;
    KartBody *body = mBody;

    if (mBody->mCarStatus & KartBody::CsUnknown16) {
        body->mBoostTimer = 45;
        return;
    }

    GetKartCtrl()->getKartSound(kartNo)->DoTandemVoice(33);
    body->mCarStatus |= body->CsUnknown15 | body->CsUnknown16;

    if (body->_590 & 2) {
        GetKartCtrl()->getKartSound(kartNo)->DoMashDashSound();
        body->mBoostTimer = 45;
        body->getStrat()->DoMotor(MotorManager::MotorType_5);
    } else {
        GetKartCtrl()->getKartSound(kartNo)->DoKartsetSeSound(0x100B3);
        body->mBoostTimer = 90;
        body->getStrat()->DoMotor(MotorManager::MotorType_8);
    }

    body->_598 = body->mBoostTimer;
    body->_52c = 0.4f;
    body->_474 = 0.313f;

    (body->_590 & 2)
        ? JPEffectPerformer::setEffect(JPEffectPerformer::Effect_Unknown18, kartNo, body->mPos, 0)
        : JPEffectPerformer::setEffect(JPEffectPerformer::Effect_Unknown19, kartNo, body->mPos, 0);

    if (!GetKartCtrl()->CheckCamera(kartNo)) {
        return;
    }

    int camNo = GetKartCtrl()->GetCameraNum(kartNo);
    if (GetKartCtrl()->getKartCam(camNo)->GetCameraMode()) {
        if (!(GetKartCtrl()->getKartCam(camNo)->GetDemoCam()->_38 & 4)) {
            return;
        }

        if (GetKartCtrl()->getKartCam(camNo)->GetCameraMode() != 0x0A) {
            return;
        }
    }

    JPEffectPerformer::setEffectEachCam(JPEffectPerformer::Effect_Unknown23, kartNo, camNo, 0);
    return;
}

void KartGame::MakeCrashDash() {}

void KartGame::MakeWheelSpin() {}

void KartGame::MakeJump() {}

void KartGame::DoAirCheck() {
    KartBody *body = mBody;
    int num = body->mMynum;

    if (!GetKartCtrl()->IsMiniGameEnd() && body->mBodyGround.getStaggerCode() != 0 && !(body->mCarStatus & 0x100000000)) {
        if (!GetKartCtrl()->getKartAnime(num)->IsDangerPlusAnime(num)) {
            if (!GetKartCtrl()->getKartAnime(num)->IsDamage(num)) {
                if (body->_460 >= 0.f) {
                    GetKartCtrl()->getKartAnime(num)->mFlags |= 0x4000000;
                }
                else {
                    GetKartCtrl()->getKartAnime(num)->mFlags |= 0x2000000;
                }
                GetKartCtrl()->getKartSound(num)->DoStaggerVoice();
            }
        }
    }

    if (!(_8 & 1) && (body->getTouchNum() == 0)) {
        if (GetKartCtrl()->getKartAnime(num)->_8 < 10.0f) {
            GetKartCtrl()->getKartAnime(num)->_8 += 0.5f;
        }
    }

    if (!(_8 & 1) && (body->getTouchNum() == 0)
        && body->mPlayerPosMtx[1][3] - body->mBodyGround.getHeight() > 150.0f 
        && body->mVel.y > 0.0f) {
       _8 |= 1;

       if (!GetKartCtrl()->getKartAnime(num)->IsJump(num)
        && !GetKartCtrl()->getKartAnime(num)->IsDangerAnime(num)
        && !GetKartCtrl()->getKartAnime(num)->IsFloatAnime(num)
        && !GetKartCtrl()->getKartAnime(num)->IsGaol(num)
        && !GetKartCtrl()->getKartAnime(num)->IsDrift(num)
        && !body->getChecker()->CheckPartsClearKey(num)) {
            if ((body->mCarStatus & 0x8000)) {
                GetKartCtrl()->getKartAnime(num)->_4 = 0.3f;
            }
            else {
                GetKartCtrl()->getKartAnime(num)->_4 = 0.2f;
            }

            GetKartCtrl()->getKartAnime(num)->mFlags |= 0x2000000000;
       }
    }

    if (((body->getTouchNum() == 0) || (body->mCarStatus & 0x40)) 
        && !(body->mCarStatus & 0x40) && body->mPlayerPosMtx[1][3] - body->mBodyGround.getHeight() > 300.0f) {
        body->mCarStatus |= 0x40;
    }
}

void KartGame::DoRearSlidePower() {
    KartBody *body = mBody;

    JGeometry::TVec3f vec0;
    JGeometry::TVec3f vec1;
    JGeometry::TVec3f vec2;
    JGeometry::TVec3f vec3;

    vec0.set(0.f, 0.f, -1.2f);

    vec1.set(-body->_35c.x * body->_3a4, 0.f, body->_35c.z * body->_3a4);

    PSMTXMultVec(body->_110, &vec0, &vec2);
    PSMTXMultVecSR(body->_110, &vec1, &vec3);

    body->DoForce(&vec2, &vec3);

    vec0.set(0.f, 0.f, 1.f);

    PSMTXMultVec(body->_110, &vec0, &vec2);
    PSMTXMultVecSR(body->_110, &vec1, &vec3);

    body->DoForce(&vec2, &vec3);
}

void KartGame::DoRearSlideBody() {
    // void JGeometry::TVec3<float>::div(float) {}
}

void KartGame::DoCorner() {}

void KartGame::FrameWork(f32, KartSus *, KartSus *) {}

void KartGame::DoBodyAction() {
    KartBody *body = mBody;
    int num = body->mMynum;

    // an array affects regalloc... maybe refactor the ones that use an array
    KartSus *sus1 = GetKartCtrl()->getKartSus(num * 4 + 0);
    KartSus *sus2 = GetKartCtrl()->getKartSus(num * 4 + 1);
    KartSus *sus3 = GetKartCtrl()->getKartSus(num * 4 + 2);
    KartSus *sus4 = GetKartCtrl()->getKartSus(num * 4 + 3);

    FrameWork(body->_3b0, sus1, sus2);
    FrameWork(body->_3b0, sus3, sus4);
    FrameWork(body->_3b0, sus1, sus4);
    FrameWork(body->_3b0, sus2, sus3);
}

void KartGame::DoElementForce() {
    KartBody *body = mBody;

    if (body->getTouchNum() != 0 || body->mCarStatus & 0x100000) {
        body->mVel.scale(0.99f);
    }
    else {
        body->mVel.scale(0.9999f);
    }
    body->mWg.scale(0.98f);
}

bool KartGame::CheckBalloon() {
    KartBody *kartBody = mBody;
    int kartNo = kartBody->mMynum;

    if (RaceMgr::getCurrentManager()->getRaceMode() != BALLOON_BATTLE) {
        return false;
    }

    if (GetKartCtrl()->HaveBalloon(kartNo) &&
        !(kartBody->mCarStatus & kartBody->CsUnknown20) &&
        !(kartBody->mCarStatus & kartBody->CsUnknown22) &&
        !(kartBody->getThunder()->mFlags & 0x01)
    ) {
        return true;
    }

    return false;
}

void KartGame::SetRank() {
    KartBody *kartBody = mBody;

    kartBody->mMyRank = RCMGetKartChecker(kartBody->mMynum)->getRank();
    kartBody->_59c = 0;
}

void KartGame::RankWatchMan() {}

void KartGame::ItemWatchMan(ItemObj *incomingItem) {
    if (!incomingItem) {
        return;
    }

    int kartNo = incomingItem->getOwnerNum();
    KartBody *kartBody = GetKartCtrl()->getKartBody(kartNo);

    RCMGetKartChecker(kartNo);
    if (kartBody->getGame()->_E) {
        return;
    }

    if (kartBody->getChecker()->CheckOnlyTandemPartsClearKey(kartNo)) {
        return;
    }

    if (GetKartCtrl()->CheckTandemItmGet(kartNo)) {
        return;
    }

    kartBody->getGame()->_E = (0x08 | 0x10 | 0x20 | 0x40);
    kartBody->getGame()->mIncomingItem = incomingItem;
}

void KartGame::AfterItemWatchMan() {
    // they probably just copy pasted the typical body myNum intro here
    // in the original code, because kartBody gets r31 assigned
    KartBody *kartBody;

    GetKartCtrl()->getKartSound(mBody->mMynum)->DoItemAlarm();
    if (_E == (0x01 | 0x02 | 0x04 | 0x10 | 0x20 | 0x40)) {
        int kartNo = mIncomingItem->getOwnerNum();
        kartBody = GetKartCtrl()->getKartBody(kartNo);

        if (mIncomingItem->getState() != ItemObj::StateDivested
                && !kartBody->getChecker()->CheckOnlyTandemPartsClearKey(kartNo)
                && !GetKartCtrl()->CheckTandemItmGet(kartNo)) {

            GetKartCtrl()->getKartSound(kartNo)->DoItmHitVoice();
            GetKartCtrl()->getKartAnime(kartNo)->mFlags |= (1LL<<34);
        }

        _E = 0;
    }

    if (_E == (0x02 | 0x04 | 0x10 | 0x20 | 0x40)) {
        _E = 0;
    }
}

void KartGame::DoFlagCtrl() {
    KartBody *kartBody = mBody;

    kartBody->_590 &= ~(0x04 | 0x08 | 0x10 | 0x40);
    kartBody->getDamage()->mFlags &= ~(0x02);
}

bool KartGame::KeepWatch() {
    KartBody *body = mBody;
    int num = body->mMynum;

    if (body->getChecker()->CheckSpecialDyn()) return true;

    int respawn = false;

    KartSus *sus[4];
    sus[0] = GetKartCtrl()->getKartSus(num * 4 + 0);
    sus[1] = GetKartCtrl()->getKartSus(num * 4 + 1);
    sus[2] = GetKartCtrl()->getKartSus(num * 4 + 2);
    sus[3] = GetKartCtrl()->getKartSus(num * 4 + 3);


    if (body->mBodyGround.getAttribute() == CrsGround::Attr_255) {
        if (sus[0]->mCrsGnd.getAttribute() == CrsGround::Attr_255 && (sus[0]->_124 & 1)
        || sus[1]->mCrsGnd.getAttribute() == CrsGround::Attr_255 && (sus[1]->_124 & 1)
        || sus[2]->mCrsGnd.getAttribute() == CrsGround::Attr_255 && (sus[2]->_124 & 1)
        || sus[3]->mCrsGnd.getAttribute() == CrsGround::Attr_255 && (sus[3]->_124 & 1)) {
            body->getRescue()->MakeRescue(nullptr, 6);
            return true;
        }
    }
    else {
        if (body->mBodyGround.getJugemPoint()) {
            u8 respawnType;
            if (body->mBodyGround.getAttribute() == CrsGround::Attr_Water) {
                if (body->_1a0[1][3] - body->mBodyGround.getWaterHeight() < -RescueSeaDepth[body->mIdx]) {
                    respawnType = 5;
                    respawn = 1;
                }
            }
            else if (body->mBodyGround.getAttribute() == CrsGround::Attr_10) {
                if ((body->mPlayerPosMtx[1][3] - body->mBodyGround.getHeight()) < 120.0f) {
                    GetKartCtrl()->getKartSound(num)->DoFallVoice();
                    
                    if (!GetKartCtrl()->getKartAnime(num)->IsDangerPlusAnime(num)) {
                        if (!GetKartCtrl()->getKartAnime(num)->IsDamage(num)) {
                            if (body->_460 >= 0.f) {
                                GetKartCtrl()->getKartAnime(num)->mFlags |= 0x4000000;
                            }
                            else {
                                GetKartCtrl()->getKartAnime(num)->mFlags |= 0x2000000;
                            }
                        }
                    }
                    respawnType = 1;
                    respawn = 1;
                }
            }
            else if (sus[0]->mCrsGnd.getAttribute() == CrsGround::Attr_5 && (sus[0]->_124 & 1)
                || sus[1]->mCrsGnd.getAttribute() == CrsGround::Attr_5 && (sus[1]->_124 & 1)
                || sus[2]->mCrsGnd.getAttribute() == CrsGround::Attr_5 && (sus[2]->_124 & 1)
                || sus[3]->mCrsGnd.getAttribute() == CrsGround::Attr_5 && (sus[3]->_124 & 1)) {
                    respawnType = 0;
                    respawn = 1;
            }
            else if (sus[0]->mCrsGnd.getAttribute() == CrsGround::Attr_15 && (sus[0]->_124 & 1)
                || sus[1]->mCrsGnd.getAttribute() == CrsGround::Attr_15 && (sus[1]->_124 & 1)
                || sus[2]->mCrsGnd.getAttribute() == CrsGround::Attr_15 && (sus[2]->_124 & 1)
                || sus[3]->mCrsGnd.getAttribute() == CrsGround::Attr_15 && (sus[3]->_124 & 1)) {
                    respawnType = 2;
                    respawn = 1;
            }
            else if (sus[0]->mCrsGnd.getAttribute() == CrsGround::Attr_14 && (sus[0]->_124 & 1)
                || sus[1]->mCrsGnd.getAttribute() == CrsGround::Attr_14 && (sus[1]->_124 & 1)
                || sus[2]->mCrsGnd.getAttribute() == CrsGround::Attr_14 && (sus[2]->_124 & 1)
                || sus[3]->mCrsGnd.getAttribute() == CrsGround::Attr_14 && (sus[3]->_124 & 1)) {
                    respawnType = 3;
                    respawn = 1;
            }
            else if (sus[0]->mCrsGnd.getAttribute() == CrsGround::Attr_255 && (sus[0]->_124 & 1)
                || sus[1]->mCrsGnd.getAttribute() == CrsGround::Attr_255 && (sus[1]->_124 & 1)
                || sus[2]->mCrsGnd.getAttribute() == CrsGround::Attr_255 && (sus[2]->_124 & 1)
                || sus[3]->mCrsGnd.getAttribute() == CrsGround::Attr_255 && (sus[3]->_124 & 1)) {
                    respawnType = 6;
                    respawn = 1;
            }

            if (respawn >= 1 && !(body->mCarStatus & 0x40000000)) {
                body->getRescue()->MakeRescue(nullptr, respawnType);
                return true;
            }
        }

        if (body->mBodyGround.getCannonPoint()) {
            body->getCannon()->MakeCannon();
            return true;
        }

        if (body->mBodyGround.getPipePoint()) {
            body->getPipe()->MakePipe();
            return true;
        }
    }

    return false;
}

void KartGame::DoActionMgr() {
    KartBody *body = mBody;
    int num = body->mMynum;
    // This makes no sense but whatever
    ItemObjMgr* itemMgr = GetItemObjMgr();
    ItemObjMgr::KartHitList *kartHitList = itemMgr->getKartHitList(num);
    GeographyObj **reactHitObjList = GetGeoObjMgr()->getKartReactHitObjectList(body->mMynum);

    if (KeepWatch()) return;

    if (!GetKartCtrl()->IsMiniGameEnd() && !(body->mGameStatus & 0x400) 
        && !(body->mCarStatus & 0x40000) && !body->getCrash()->CheckMatchlessTimer()) {
        
        if (reactHitObjList[0x10]) {
            body->getCrash()->MakeTornado();
        } else if (reactHitObjList[17]) {
            body->getRescue()->MakeRescue(reactHitObjList[17], 4);
        }
        else if (kartHitList->mObjects[9]) {
            body->getTumble()->MakeWanWanTumble(kartHitList->mObjects[9]);
        }
        else if (kartHitList->mObjects[4]) {
            body->getCrash()->MakeKameCrash(kartHitList->mObjects[4]);
        }
        else if (kartHitList->mObjects[3]) {
            body->getCrash()->MakeKameCrash(kartHitList->mObjects[3]);
        }
        else if (kartHitList->mObjects[8]) {
            body->getCrash()->MakeBombCrash(kartHitList->mObjects[8]);
        }
        else if (kartHitList->mObjects[2]) {
            body->getCrash()->MakeSpin(kartHitList->mObjects[2]);
        }
        else if (kartHitList->mObjects[0]) {
            body->getCrash()->MakeHalfSpin(kartHitList->mObjects[0]);
        }
        else if (kartHitList->mObjects[11]) {
            body->getTumble()->MakeKameTumble(kartHitList->mObjects[11]);
        }
        else if (kartHitList->mObjects[10]) {
            body->getCrash()->MakeBurn(kartHitList->mObjects[10]);
        }
        else if (kartHitList->mObjects[12]) {
            body->getThunder()->MakeThunder();
        }
        else if (body->_590 & 4) {
            body->getCrash()->MakeSpin(nullptr);
        }
        else if (body->_590 & 8) {
            body->getCrash()->MakeHalfSpin(nullptr);
        }

        if (body->mCarStatus & 0x100000) return;

        if (body->_590 & 0x10) {
            body->getTumble()->MakeStarTumble();
        }
        else if (body->getDamage()->mFlags & 4) {
            body->getCrash()->MakeHalfSpin(nullptr);
        }
        else if (body->getDamage()->mFlags & 2) {
            body->getDamage()->MakeHalfDamage();
        }
        else if (body->getChecker()->CheckCrash() == true) {
            return;
        }
    }

    DoRollOver();
    if (kartHitList->mObjects[7]) {
        body->getStar()->MakeStar();
    }
    else if (kartHitList->mObjects[6]) {
        MakeMashDash();
    }
    else if (body->mBodyGround.isObject() && body->getTouchNum() >= 1) {
        switch (body->mBodyGround.getObject()->getKind()) {
        case 0x3fd:
            MakeBoardDash();
            break;
        }
    }
    else {
        if (body->mBodyGround.getAttribute() == CrsGround::Attr_7  && body->getTouchNum() >= 1) {
            MakeJumpDash();
        }
        if (body->mBodyGround.getAttribute() == CrsGround::Attr_55  && body->getTouchNum() >= 1) {
            MakeSpJumpDash();
        }   
        if (body->mBodyGround.getAttribute() == CrsGround::Attr_71  && body->getTouchNum() >= 1) {
            MakeSpJumpDash();
        }
        if (body->mBodyGround.getAttribute() == CrsGround::Attr_8  && body->getTouchNum() >= 1) {
            MakeBoardDash();
        }
    }
}

void KartGame::DoActionCtrl() {
    // peak mwcc doing an extra compare when checking explicit against true
    if (mBody->getChecker()->CheckCrash() == true) {
        return;
    }

    DoSlide();
    DoWanWan();
}

void KartGame::DoStatus() {
    KartBody *body = mBody;
    int num = body->mMynum;

    if (!body->getChecker()->CheckIndication()) {
        if ((body->mGameStatus & 0x4000) != 0) {
            DoStartGoalCtl();
        }
        else {
            return;
        }
    }
    else {
        DoStartGoalCtl();
    }
    body->DegubBody(0);
    if ((body->mGameStatus & KartBody::GsUnknown3) == 0) {
        WatchEffectAcceleration();
    }
    body->DegubBody(1);
    WatchAcceleration();
    body->DegubBody(2);
    if ((body->mGameStatus & KartBody::GsUnknown3) == 0) {
        GetKartCtrl()->getKartSound(num)->DoInfo(num);
    }

    body->DegubBody(3);
    GetKartCtrl()->DoContCtl(num);
    body->DegubBody(4);
    if ((body->mGameStatus & KartBody::GsUnknown3) != 0) {
        GetKartCtrl()->getKartSound(num)->DoInfo(num);
    }
    if ((body->mGameStatus & KartBody::GsUnknown3) != 0) {
        WatchEffectAcceleration();
    }
    GetGorundTireNum();
    body->DegubBody(5);
    if ((body->mGameStatus & KartBody::GsUnknown2) != 0) {
        body->getItem()->CheckOtherAnime();
        body->DegubBody(6);
        if (!body->getChecker()->CheckCrash()) {
            GetKartCtrl()->getKartSound(num)->DoHorn();
            body->DegubBody(7);
            DoRoll();
            body->DegubBody(8);
            if ((body->mGameStatus & KartBody::GsUnknown3) == 0) {
                DoTestPitch();
            }
            body->DegubBody(9);
            DoLiftTurbo();
            body->DegubBody(10);
            if ((body->mCarStatus & KartBody::Drifting) == 0) {
                DoTurbo();
            }
            body->DegubBody(0xb);
            if ((body->mGameStatus & KartBody::GsUnknown3) == 0) {
                DoCorner();
            }
            body->DegubBody(0xc);
            if ((body->mGameStatus & KartBody::GsUnknown3) == 0) {
                DoRearSlideBody();
            }
            body->DegubBody(0xd);
            DoBodyAction();
            body->DegubBody(0xe);
        }
        DoAirCheck();
        body->DegubBody(0xf);
        DoChange();
        body->DegubBody(0x10);
        DoActionCtrl();
        body->DegubBody(0x11);
        if (!body->getChecker()->CheckCrash()) {
            body->getItem()->DoItem();
        }
        body->DegubBody(0x12);
        GetKartCtrl()->DoAnime(num);
        body->DegubBody(0x13);
        DoActionMgr();
        body->DegubBody(0x14);
        DoFlagCtrl();
        body->DegubBody(0x15);
        if ((body->mCarStatus & KartBody::CsUnknown22) != 0) {
            body->getRescue()->DoRescueCrl();
        }
        body->DegubBody(0x16);
        if ((body->mCarStatus & 0x8100000000) != 0) {
            body->getCannon()->DoCannonCrl();
        }
        body->DegubBody(0x17);
        if ((body->mCarStatus & 0x100000000000) != 0) {
            body->getPipe()->DoPipeCrl();
        }
            body->DegubBody(0x18);
        if ((body->mCarStatus & 0x200000000000) != 0) {
            body->getAnt()->DoAntCrl();
        }
        body->DegubBody(0x19);
        if ((body->mCarStatus & 0x200000000) != 0) {
            body->getDossin()->DoDossinCrl();
        }
        body->DegubBody(0x1a);
        body->getDossin()->DoDossinTimer();
        body->DegubBody(0x1b);
        if ((body->mCarStatus & 0x40000) != 0) {
            body->getStar()->DoStarCrl();
        }
        body->DegubBody(0x1c);
        if (body->getTumble()->mFlags & 1) {
            body->getTumble()->DoTumbleCrl();
        }
        body->DegubBody(0x1d);
        if ((body->getThunder()->mFlags & 1) != 0) {
            body->getThunder()->DoThunderCrl();
        }
        body->DegubBody(0x1e);
        if ((body->getDamage()->mFlags & 0x170) != 0) {
            body->getDamage()->DoDamageCrl();
        }
        body->DegubBody(0x1f);
        body->getGame()->AfterItemWatchMan();
        body->DegubBody(0x20);
    }
}
