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

#ifndef __XThread_h__
#define __XThread_h__

#include <CarbonCore/MacTypes.h>
#include <pthread.h>

class XThread {
public:
	enum EPolicy {
		kPolicyTimeslice,
		kPolicyRoundRobin,
		kPolicyFIFO
	};
	
	enum EPriorityGroup {
		kPriorityUser,
		kPriorityServer,
		kPrioritySystem
	};
	
	XThread(EPriorityGroup group, int priority = 0, EPolicy policy = kPolicyTimeslice, int rrQuantumMs = 1);
				// priority must be 0...GetNumPriorities(group)-1
				// rrQuantumMs is quantum for round-robin scheduling, in milliseconds (I think!)
				//		(cf thread_policy_common in mk_sp.c, time.h, tick is microseconds per Hz tick)
	virtual ~XThread();
	
	OSStatus		Start();
						// return non-zero if error
	virtual void	Run() = 0;

	static int	GetNumPriorities(EPriorityGroup group);

private:
	EPriorityGroup mGroup;
	EPolicy		mPolicy;
	int			mPriority;
	int			mQuantumMs;
	pthread_t	mThread;
	
	static void *	RunHelper(void *param);
};


#endif // __XThread_h__
