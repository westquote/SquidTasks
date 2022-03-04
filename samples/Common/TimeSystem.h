#pragma once

#include <chrono>
#include <atomic>

//--- Time System ---//
class TimeSystem
{
public:
	template <typename... Args>
	static void Create(Args&&... args)
	{
		s_timeSys = new TimeSystem(std::forward<Args>(args)...);
	}
	static void UpdateTime()
	{
		s_timeSys->_UpdateTime();
	}
	static auto GetTimeSince(double in_time)
	{
		return s_timeSys->_GetTimeSince(in_time);
	}
	static auto GetTime()
	{
		return s_timeSys->_GetTime();
	}

private:
	TimeSystem()
	{
		m_startTimePoint = std::chrono::steady_clock::now();
	}
	void _UpdateTime()
	{
		std::chrono::steady_clock::time_point curTimePoint = std::chrono::steady_clock::now();
		std::chrono::duration<double> span = std::chrono::duration_cast<std::chrono::duration<double>>(curTimePoint - m_startTimePoint);
		m_time = span.count();
	}
	double _GetTimeSince(double in_time) const
	{
		return m_time - in_time;
	}
	double _GetTime() const
	{
		return m_time;
	}

	std::chrono::steady_clock::time_point m_startTimePoint;
	std::atomic<double> m_time;
	static TimeSystem* s_timeSys;
};
TimeSystem* TimeSystem::s_timeSys;
