#ifndef _SM_WAVE_H_
#define _SM_WAVE_H_

#include "../animation.h"
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include "../animationstation.h"
#include "../specialmovesystem.h"

typedef enum
{
  SM_WAVE_STATE_NOTSTARTED,
  SM_WAVE_STATE_PREFRAMES,
  SM_WAVE_STATE_RUNNING,
  SM_WAVE_STATE_POSTFRAMES,
  SM_WAVE_STATE_FINISHED,
} SMWaveState;

class SMWave : public Animation {
public:
    SMWave(Lights& InRGBLights, EButtonCaseEffectType InButtonCaseEffectType);
    ~SMWave() { };

    virtual void Animate(RGB (&frame)[FRAME_MAX]) override;

    virtual void SetOptionalParams(uint32_t OptionalParams) override;
    virtual bool IsFinished() override; 

protected:

    float GetWaveSpeedFromEnum();
    bool CheckDividerForLight(unsigned int lightIndex);
    bool UpdateWave(RGB (&frame)[FRAME_MAX]);
    float GetStrengthForLight(int XPos, int YPos);

    SpecialMoveAnimationDirection WaveDirection;
    SpecialMoveAnimationDuration WaveSpeed;
    SpecialMoveAnimationDivider WaveDivider;
    int WaveLoops = 0;
    uint8_t ColorIndex;
    
    int MinXCoord = 0;
    int MinYCoord = 0;
    int MaxXCoord = 0;
    int MaxYCoord = 0;
    int MidXCoord = 0;
    int MidYCoord = 0;

    float WaveValue = 0.0f;

    uint32_t CurrentStateTime = 0;
    SMWaveState CurrentState = SMWaveState::SM_WAVE_STATE_NOTSTARTED;
};

#endif
