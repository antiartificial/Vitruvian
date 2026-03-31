/*
 * Copyright 2026, The Vitruvian Project. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Tier-0 (Docker / no nexus kernel module) stub implementations of the
 * Haiku kernel API functions and globals that are normally provided by
 * thread.cpp, sem.cpp, port.cpp, area.cpp, and Team.cpp.
 *
 * Every function returns B_NOT_SUPPORTED so that the rest of the OS builds
 * and links cleanly.  Nothing here is intended to run; it exists solely to
 * satisfy the linker when nexus is absent.
 */

#include <OS.h>
#include <pthread.h>
#include <sys/stat.h>
#include <libudev.h>

#include "Team.h"

namespace BKernelPrivate {

void
Team::InitTeam()
{
}

int
Team::GetNexusDescriptor()
{
	return -1;
}

int
Team::GetSemDescriptor()
{
	return -1;
}

int
Team::GetAreaDescriptor()
{
	return -1;
}

int
Team::GetVRefDescriptor(dev_t*)
{
	return -1;
}

int
Team::GetNodeMonitorDescriptor()
{
	return -1;
}

struct udev*
Team::GetUDev()
{
	return nullptr;
}

mode_t
Team::GetUmask()
{
	return 022;
}

int32
Team::GetCPUCount()
{
	return 1;
}

thread_id
Team::LoadImage(int32, const char**, const char**)
{
	return B_NOT_SUPPORTED;
}

void
Team::PrepareFatherAtFork()
{
}

void
Team::SyncFatherAtFork()
{
}

void
Team::ReinitChildAtFork()
{
}

}	// namespace BKernelPrivate

// ---------------------------------------------------------------------------
// Globals (defined in Team.cpp when nexus is present)
// ---------------------------------------------------------------------------

int32	__gCPUCount = 1;
mode_t	__gUmask    = 022;
int		__libc_argc = 0;
char**	__libc_argv = nullptr;
char**	argv_save   = nullptr;

// ---------------------------------------------------------------------------
// Threads
// ---------------------------------------------------------------------------

thread_id
spawn_thread(thread_func, const char*, int32, void*)
{
	return B_NOT_SUPPORTED;
}

status_t
kill_thread(thread_id)
{
	return B_NOT_SUPPORTED;
}

status_t
resume_thread(thread_id)
{
	return B_NOT_SUPPORTED;
}

void
exit_thread(status_t)
{
}

status_t
wait_for_thread(thread_id, status_t*)
{
	return B_NOT_SUPPORTED;
}

thread_id
find_thread(const char*)
{
	return B_NOT_SUPPORTED;
}

status_t
rename_thread(thread_id, const char*)
{
	return B_NOT_SUPPORTED;
}

status_t
set_thread_priority(thread_id, int32)
{
	return B_NOT_SUPPORTED;
}

status_t
suspend_thread(thread_id)
{
	return B_NOT_SUPPORTED;
}

status_t
on_exit_thread(void (*)(void*), void*)
{
	return B_NOT_SUPPORTED;
}

bool
has_data(thread_id)
{
	return false;
}

status_t
receive_data(thread_id*, void*, size_t)
{
	return B_NOT_SUPPORTED;
}

status_t
send_data(thread_id, int32, const void*, size_t)
{
	return B_NOT_SUPPORTED;
}

status_t
snooze(bigtime_t)
{
	return B_NOT_SUPPORTED;
}

status_t
snooze_until(bigtime_t, int)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_thread_info(thread_id, thread_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

// ---------------------------------------------------------------------------
// Semaphores
// ---------------------------------------------------------------------------

sem_id
create_sem(int32, const char*)
{
	return B_NOT_SUPPORTED;
}

status_t
delete_sem(sem_id)
{
	return B_NOT_SUPPORTED;
}

status_t
acquire_sem(sem_id)
{
	return B_NOT_SUPPORTED;
}

status_t
acquire_sem_etc(sem_id, int32, uint32, bigtime_t)
{
	return B_NOT_SUPPORTED;
}

status_t
release_sem(sem_id)
{
	return B_NOT_SUPPORTED;
}

status_t
release_sem_etc(sem_id, int32, uint32)
{
	return B_NOT_SUPPORTED;
}

status_t
get_sem_count(sem_id, int32*)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_sem_info(sem_id, sem_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

// ---------------------------------------------------------------------------
// Ports
// ---------------------------------------------------------------------------

port_id
create_port(int32, const char*)
{
	return B_NOT_SUPPORTED;
}

status_t
delete_port(port_id)
{
	return B_NOT_SUPPORTED;
}

port_id
find_port(const char*)
{
	return B_NOT_SUPPORTED;
}

ssize_t
read_port(port_id, int32*, void*, size_t)
{
	return B_NOT_SUPPORTED;
}

ssize_t
read_port_etc(port_id, int32*, void*, size_t, uint32, bigtime_t)
{
	return B_NOT_SUPPORTED;
}

status_t
write_port(port_id, int32, const void*, size_t)
{
	return B_NOT_SUPPORTED;
}

status_t
write_port_etc(port_id, int32, const void*, size_t, uint32, bigtime_t)
{
	return B_NOT_SUPPORTED;
}

ssize_t
port_count(port_id)
{
	return B_NOT_SUPPORTED;
}

ssize_t
port_buffer_size(port_id)
{
	return B_NOT_SUPPORTED;
}

ssize_t
port_buffer_size_etc(port_id, uint32, bigtime_t)
{
	return B_NOT_SUPPORTED;
}

status_t
set_port_owner(port_id, team_id)
{
	return B_NOT_SUPPORTED;
}

status_t
close_port(port_id)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_port_info(port_id, port_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_port_message_info_etc(port_id, port_message_info*, size_t, uint32, bigtime_t)
{
	return B_NOT_SUPPORTED;
}

// ---------------------------------------------------------------------------
// Areas
// ---------------------------------------------------------------------------

area_id
create_area(const char*, void**, uint32, size_t, uint32, uint32)
{
	return B_NOT_SUPPORTED;
}

area_id
clone_area(const char*, void**, uint32, uint32, area_id)
{
	return B_NOT_SUPPORTED;
}

status_t
delete_area(area_id)
{
	return B_NOT_SUPPORTED;
}

status_t
set_area_protection(area_id, uint32)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_area_info(area_id, area_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

// ---------------------------------------------------------------------------
// Vrefs
// ---------------------------------------------------------------------------

vref_id
create_vref(int)
{
	return B_NOT_SUPPORTED;
}

status_t
acquire_vref(vref_id)
{
	return B_NOT_SUPPORTED;
}

status_t
release_vref(vref_id)
{
	return B_NOT_SUPPORTED;
}

int
open_vref(vref_id)
{
	return B_NOT_SUPPORTED;
}

dev_t
get_vref_dev()
{
	return (dev_t)B_NOT_SUPPORTED;
}

// ---------------------------------------------------------------------------
// Team / misc
// ---------------------------------------------------------------------------

status_t
_get_team_info(team_id, team_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_next_team_info(int32*, team_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

status_t
kill_team(team_id)
{
	return B_NOT_SUPPORTED;
}

status_t
resize_area(area_id, size_t)
{
	return B_NOT_SUPPORTED;
}

// ---------------------------------------------------------------------------
// Remaining OS.h API — not nexus-dependent but missing from libroot2 sources
// ---------------------------------------------------------------------------

area_id
find_area(const char*)
{
	return B_NOT_SUPPORTED;
}

area_id
area_for(void*)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_next_area_info(team_id, ssize_t*, area_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_next_port_info(team_id, int32*, port_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

status_t
set_sem_owner(sem_id, team_id)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_next_sem_info(team_id, int32*, sem_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_team_usage_info(team_id, int32, team_usage_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

status_t
snooze_etc(bigtime_t, int, uint32)
{
	return B_NOT_SUPPORTED;
}

status_t
_get_next_thread_info(team_id, int32*, thread_info*, size_t)
{
	return B_NOT_SUPPORTED;
}

thread_id
get_pthread_thread_id(pthread_t)
{
	return B_NOT_SUPPORTED;
}

status_t
convert_to_pthread(thread_id, pthread_t*)
{
	return B_NOT_SUPPORTED;
}

status_t
set_timezone(const char*)
{
	return B_NOT_SUPPORTED;
}

bigtime_t
set_alarm(bigtime_t, uint32)
{
	return 0;
}

status_t
acquire_vref_etc(vref_id, int*)
{
	return B_NOT_SUPPORTED;
}

// ---------------------------------------------------------------------------
// Kernel calls missing from nexus-guarded files
// ---------------------------------------------------------------------------

status_t
_kern_reserve_address_range(addr_t*, uint32, addr_t)
{
	return B_NOT_SUPPORTED;
}

area_id
_kern_transfer_area(area_id, void**, uint32, team_id)
{
	return B_NOT_SUPPORTED;
}

status_t
_kern_start_watching(dev_t, ino_t, uint32, port_id, uint32)
{
	return B_NOT_SUPPORTED;
}

status_t
_kern_stop_watching(dev_t, ino_t, port_id, uint32)
{
	return B_NOT_SUPPORTED;
}

status_t
_kern_stop_notifying(port_id, uint32)
{
	return B_NOT_SUPPORTED;
}
