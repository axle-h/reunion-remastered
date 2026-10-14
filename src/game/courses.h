// Improvement (game/improvements.h, university_courses): Universities that
// count. In the original a course needs no University and only one staff
// member can be away at a time. With the switch on, a course needs a
// finished, powered University at any colony, counted when the advisor is
// asked (Sim_UniversityCount, which nothing reads, stays as it is), and
// with N of them up to N staff can be away together, one per kind. New
// Earth starts with one, so a new game sends one at a time, as the
// original does. Course length, price and results are the original's. Courses already running carry on if
// Universities are lost or lose power, and with the switch off; only a new
// course checks.
//
// State: one course per staff kind. The DOS fields hold one
// (Advisor_TrainingKind, Timer and Course); the others are port records,
// Courses_Extra, by kind. A new course takes the DOS fields when they are
// free; when their course ends (it returns, or Hire_HireScreen replaces the
// staff member) the lowest kind of the records moves into them. So one
// running course is always in the DOS fields, and only a save holding more
// is in the port's format (game/saveload.h). The quoted price stays the one
// Advisor_TrainingPrice: it is paid in the same talk.
//
// Every place that asks who is away goes through Courses_Away.
#pragma once

#include <stdint.h>

// Bytes only, so the record is its own file layout (saves).
typedef struct Course {
    uint8_t Hours;   // left at the university; 0 = not away
    uint8_t Course;  // the developer's course 1..4 (Advisor_CourseSkills)
} Course;
static_assert(sizeof(Course) == 2);

enum : int { COURSE_KINDS = 4 };

// The courses beyond the DOS fields' one, [kind - 1].
extern Course Courses_Extra[COURSE_KINDS];

// Every record cleared (a new game, a DOS save or one without them loaded).
void Courses_Reset(void);
// A record is in use: saves are in the port's format.
bool Courses_Any(void);

// Staff kind `kind` (1..4) is at the university.
bool Courses_Away(int16_t kind);
// Finished, powered Universities at any colony.
int16_t Courses_Universities(void);
// The advisor's first answer to "Go to university?" (valasz1.sp line, or
// ADVISOR_NO_UNIVERSITY): `free` (the original's 10 or 0Eh) while one more
// can go, else 9; with the switch on and no University, the port's answer.
int16_t Courses_Answer(int16_t free);
// A paid course of `hours` for the staff member being talked to
// (Command_StaffPicked); the developer's course is Advisor_TrainingCourse.
void Courses_Start(int16_t kind, int16_t hours);
// The course of staff kind `kind` ends without results (the staff member
// is replaced).
void Courses_End(int16_t kind);
// The DOS fields take a record's course once theirs has ended.
void Courses_Settle(void);
