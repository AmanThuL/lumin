//*******************************************************************
// Copyright Frank Luna (C) 2011 All Rights Reserved.
//
// GameTimer.h:
//
// Measures the total time since the application started, and the time
// elapsed between frames.
//*******************************************************************

#ifndef GAMETIMER_H
#define GAMETIMER_H

class GameTimer
{
public:
    /// <summary>
    /// Constructor that queries the frequency of the performance counter.
    /// </summary>
    GameTimer();


    /// <returns>
    /// Returns the total time (in seconds) elapsed since Reset() was called. 
    /// NOT counting any time when the clock is stopped.
    /// </returns>
    float TotalTime()const;

    /// <returns>Returns the delta-time in seconds.</returns>
    float DeltaTime() const;

    
    /// <summary>Call before message loop.</summary>
    void Reset();

    /// <summary>Call when unpaused.</summary>
    void Start();
    
    /// <summary>Call when paused.</summary>
    void Stop();
    
    /// <summary>Call every frame.</summary>
    void Tick();

private:
    double mSecondsPerCount;
    double mDeltaTime;

    __int64 mBaseTime;
    __int64 mPausedTime;
    __int64 mStopTime;
    __int64 mPrevTime;
    __int64 mCurrTime;

    bool mStopped;
};

#endif // GAMETIMER_H

