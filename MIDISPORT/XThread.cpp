/*
	Copyright (c) 2000 Apple Computer, Inc., All Rights Reserved.

	You may incorporate this Apple sample source code into your program(s) without
	restriction. This Apple sample source code has been provided "AS IS" and the
	responsibility for its operation is yours. You are not permitted to redistribute
	this Apple sample source code as "Apple sample source code" after having made
	changes. If you're going to re-distribute the source, we require that you make
	it clear in the source that the code was descended from Apple sample source
	code, but that you've made changes.
	
	NOTE: THIS IS EARLY CODE, NOT NECESSARILY SUITABLE FOR SHIPPING PRODUCTS.
	IT IS INTENDED TO GIVE HARDWARE DEVELOPERS SOMETHING WITH WHICH TO GET
	DRIVERS UP AND RUNNING AS SOON AS POSSIBLE.
	
	In particular, the implementation is much more complex and ugly than is
	necessary because of limitations of I/O Kit's USB user client code in DP4.
	As I/O Kit evolves, this code will be updated to be much simpler.
*/

#include "XThread.h"
#include <pthread.h>
#include <CarbonCore/MacErrors.h>
#include <mach/thread_act.h>
#include <mach/mach.h>

XThread::XThread(EPriorityGroup group, int priority, EPolicy policy, int quantumMs) : 
	 mGroup(group), mPolicy(policy), mQuantumMs(quantumMs)
{
	int maxPrio = GetNumPriorities(group);
	if (priority >= maxPrio)
		priority = maxPrio - 1;

	host_priority_info pi;
	mach_msg_type_number_t n;
	/*kern_return_t ret =*/ host_info(mach_host_self(), HOST_PRIORITY_INFO,
		(host_info_t)&pi, &n);
	switch (group) {
	case kPriorityUser:
		mPriority = pi.user_priority + priority;
		break;
	case kPriorityServer:
		mPriority = pi.server_priority + priority;
		break;
	case kPrioritySystem:
		mPriority = pi.system_priority + priority;
		break;
	default:
		mPriority = pi.user_priority;
		break;
	}
}

XThread::~XThread()
{
}

OSStatus	XThread::Start()
{
	int result;
	pthread_attr_t attr;
	policy_base_data_t base;
	
	result = pthread_attr_init(&attr);
	if (result) return kMPInsufficientResourcesErr;
	
	result = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (result) return kMPInsufficientResourcesErr;
	
	result = pthread_create(&mThread, &attr, XThread::RunHelper, this);

	pthread_attr_destroy(&attr);

	if (result) return kMPInsufficientResourcesErr;
	
	switch (mPolicy) {
	case kPolicyFIFO:
		{
			base.fifo.base_priority = mPriority;
			result = thread_policy(pthread_mach_thread_np(mThread),
									POLICY_FIFO,
									(policy_base_t) &base.fifo,
									POLICY_FIFO_BASE_COUNT,
									TRUE);
		}
		break;
	case kPolicyRoundRobin:
		{
			base.rr.base_priority = mPriority;
			base.rr.quantum = mQuantumMs;
			result = thread_policy(pthread_mach_thread_np(mThread),
									POLICY_RR,
									(policy_base_t) &base.rr,
									POLICY_RR_BASE_COUNT,
									TRUE);
		}
		break;
	case kPolicyTimeslice:
		{
			base.ts.base_priority = mPriority;
			result = thread_policy(pthread_mach_thread_np(mThread),
									POLICY_TIMESHARE,
									(policy_base_t) &base.ts,
									POLICY_TIMESHARE_BASE_COUNT,
									TRUE);
		}
		break;
	default:
		return kMPInsufficientResourcesErr;
	}
	if (result)
		return kMPInsufficientResourcesErr;
	
	return noErr;
}

void *	XThread::RunHelper(void * param)
{
	static_cast<XThread *>(param)->Run();
	return NULL;
}

int		XThread::GetNumPriorities(EPriorityGroup group)
{
	host_priority_info pi;
	mach_msg_type_number_t n;
	/*kern_return_t ret =*/ host_info(mach_host_self(), HOST_PRIORITY_INFO,
		(host_info_t)&pi, &n);
	switch (group) {
	case kPriorityUser:
		return pi.server_priority - pi.user_priority;
	case kPriorityServer:
		return pi.system_priority - pi.server_priority;
	case kPrioritySystem:
		return pi.kernel_priority - pi.system_priority;
	}
	return 0;
}
