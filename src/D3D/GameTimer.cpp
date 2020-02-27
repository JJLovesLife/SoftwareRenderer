#include "GameTimer.h"
#include <Windows.h>

GameTimer::GameTimer() :
	mSecondsPerCount(0.0), mDeltaTime(-1.0f), mBaseTime(0),
	mPausedTime(0),	mPrevTime(0), mCurrTime(0), mStopped(false)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSecondsPerCount = 1.0 / (double)countsPerSec;
}

float GameTimer::TotalTime() const {
	if (mStopped) {
		return (float)(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	}
	else {
		return (float)(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	}
}

void GameTimer::Reset() {
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;
}

void GameTimer::Start() {
	if (mStopped) {
		__int64 startTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
		mPausedTime += (startTime - mStopTime);
		mPrevTime = startTime;
		mStopTime = 0;
		mStopped = false;
	}
}

void GameTimer::Stop() {
	if (!mStopped) {
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		mStopTime = currTime;
		mStopped = true;

		// JJ
		// Q: if it is needed to store this duration to mPrevTime,
		// this the time between last tick and currTime is also part of the DeltaTime.
	}
}

void GameTimer::Tick() {
	if (mStopped) {
		mDeltaTime = 0.0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;
	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;
	mPrevTime = mCurrTime;
	
	// if the processor goes into a power save mode or
	// we get shuffled to another processor,
	// then mDeltaTime can be negative.
	if (mDeltaTime < 0.0) {
		mDeltaTime = 0.0;
	}
}
