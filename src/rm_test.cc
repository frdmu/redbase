//
// File:        rm_testshell.cc
// Description: Test RM component
// Authors:     Jan Jannink
//              Dallan Quass (quass@cs.stanford.edu)
//              Jason McHugh (mchughj@cs.stanford.edu)
//
// This test shell contains a number of functions that will be useful
// in testing your RM component code.  In addition, a couple of sample
// tests are provided.  The tests are by no means comprehensive, however,
// and you are expected to devise your own tests to test your code.
//
// 1997:  Tester has been modified to reflect the change in the 1997
// interface.  For example, FileHandle no longer supports a Scan over the
// relation.  All scans are done via a FileScan.
//

#include <cstdio>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cstdlib>
#include <cassert>
#include "redbase.h"
#include "pf.h"
#include "rm.h"

using namespace std;

//
// Defines
//
#define FILENAME   "testrel"         // test file name
#define STRLEN      29               // length of string in testrec
#define PROG_UNIT   500               // how frequently to give progress
                                      //   reports when adding lots of recs
#define FEW_RECS   20                // number of records added in
#define LOTS_OF_RECS 12345
//
// Computes the offset of a field in a record (should be in <stddef.h>)
//
#ifndef offsetof
#       define offsetof(type, field)   ((size_t)&(((type *)0) -> field))
#endif

//
// Structure of the records we will be using for the tests
//
struct TestRec {
    char  str[STRLEN];
    int   num;
    float r;
};

//
// Global PF_Manager and RM_Manager variables
//
PF_Manager pfm;
RM_Manager rmm(pfm);

//
// Function declarations
//
RC Test1(void);
RC Test2(void);
RC Test3(void);
RC Test4(void);
RC Test5(void);
RC Test6(void);
RC Test7(void);
void PrintError(RC rc);
void LsFile(char *fileName);
void PrintRecord(TestRec &recBuf);
RC AddRecs(RM_FileHandle &fh, int numRecs);
RC VerifyFile(RM_FileHandle &fh, int numRecs);
RC PrintFile(RM_FileHandle &fh);

RC CreateFile(char *fileName, int recordSize);
RC DestroyFile(char *fileName);
RC OpenFile(char *fileName, RM_FileHandle &fh);
RC CloseFile(char *fileName, RM_FileHandle &fh);
RC InsertRec(RM_FileHandle &fh, char *record, RID &rid);
RC UpdateRec(RM_FileHandle &fh, RM_Record &rec);
RC DeleteRec(RM_FileHandle &fh, RID &rid);
RC GetNextRecScan(RM_FileScan &fs, RM_Record &rec);

//
// Array of pointers to the test functions
//
#define NUM_TESTS       7               // number of tests
int (*tests[])() =                      // RC doesn't work on some compilers
{
    Test1,
    Test2,
    Test3,
    Test4,
    Test5,
    Test6,
	Test7
};

//
// main
//
int main(int argc, char *argv[])
{
	RC   rc;
    char *progName = argv[0];   // since we will be changing argv
    int  testNum;

    // Write out initial starting message
    cerr.flush();
    cout.flush();
    cout << "Starting RM component test.\n";
    cout.flush();

    // Delete files from last time
    unlink(FILENAME);

    // If no argument given, do all tests
    if (argc == 1) {
        for (testNum = 0; testNum < NUM_TESTS; testNum++)
            if ((rc = (tests[testNum])())) {

                // Print the error and exit
                PrintError(rc);
                return (1);
            }
    }
    else {

        // Otherwise, perform specific tests
        while (*++argv != NULL) {

            // Make sure it's a number
            if (sscanf(*argv, "%d", &testNum) != 1) {
                cerr << progName << ": " << *argv << " is not a number\n";
                continue;
            }

            // Make sure it's in range
            if (testNum < 1 || testNum > NUM_TESTS) {
                cerr << "Valid test numbers are between 1 and " << NUM_TESTS << "\n";
                continue;
            }

            // Perform the test
            if ((rc = (tests[testNum - 1])())) {

                // Print the error and exit
                PrintError(rc);
                return (1);
            }
        }
    }

    // Write ending message and exit
    cout << "Ending RM component test.\n\n";

    return (0);
}

//
// PrintError
//
// Desc: Print an error message by calling the proper component-specific
//       print-error function
//
void PrintError(RC rc)
{
    if (abs(rc) <= END_PF_WARN)
        PF_PrintError(rc);
    else if (abs(rc) <= END_RM_WARN)
        RM_PrintError(rc);
    else
        cerr << "Error code out of range: " << rc << "\n";
}

////////////////////////////////////////////////////////////////////
// The following functions may be useful in tests that you devise //
////////////////////////////////////////////////////////////////////

//
// LsFile
//
// Desc: list the filename's directory entry
//
void LsFile(char *fileName)
{
    char command[80];

    sprintf(command, "ls -l %s", fileName);
    printf("doing \"%s\"\n", command);
    system(command);
}

//
// PrintRecord
//
// Desc: Print the TestRec record components
//
void PrintRecord(TestRec &recBuf)
{
    printf("[%s, %d, %f]\n", recBuf.str, recBuf.num, recBuf.r);
}

//
// AddRecs
//
// Desc: Add a number of records to the file
//
RC AddRecs(RM_FileHandle &fh, int numRecs)
{
    RC      rc;
    int     i;
    TestRec recBuf;
    RID     rid;
    PageNum pageNum;
    SlotNum slotNum;

    // We set all of the TestRec to be 0 initially.  This heads off
    // warnings that Purify will give regarding UMR since sizeof(TestRec)
    // is 40, whereas actual size is 37.
    memset((void *)&recBuf, 0, sizeof(recBuf));

    printf("\nadding %d records\n", numRecs);
    for (i = 0; i < numRecs; i++) {
        memset(recBuf.str, ' ', STRLEN);
        sprintf(recBuf.str, "a%d", i);
        recBuf.num = i;
        recBuf.r = (float)i;
        if ((rc = InsertRec(fh, (char *)&recBuf, rid)) ||
            (rc = rid.GetPageNum(pageNum)) ||
            (rc = rid.GetSlotNum(slotNum)))
            return (rc);

        if ((i + 1) % PROG_UNIT == 0){
            printf("%d  ", i + 1);
            fflush(stdout);
        }
    }
    if (i % PROG_UNIT != 0)
        printf("%d\n", i);
    else
        putchar('\n');

    printf("Page/Slot: %d %d\n", pageNum, slotNum);
    // Return ok
    return (0);
}

//
// VerifyFile
//
// Desc: verify that a file has records as added by AddRecs
//
RC VerifyFile(RM_FileHandle &fh, int numRecs)
{
    RC        rc;
    int       n;
    TestRec   *pRecBuf;
    RID       rid;
    char      stringBuf[STRLEN];
    char      *found;
    RM_Record rec;

    found = new char[numRecs];
    memset(found, 0, numRecs);

    printf("\nverifying file contents\n");

    RM_FileScan fs;
    if ((rc=fs.OpenScan(fh,INT,sizeof(int),offsetof(TestRec, num),
                        NO_OP, NULL, NO_HINT)))
        return (rc);

    // For each record in the file
    for (rc = GetNextRecScan(fs, rec), n = 0;
         rc == 0;
         rc = GetNextRecScan(fs, rec), n++) {

        // Make sure the record is correct
        if ((rc = rec.GetData((char *&)pRecBuf)) ||
            (rc = rec.GetRid(rid)))
            goto err;

        memset(stringBuf,' ', STRLEN);
        sprintf(stringBuf, "a%d", pRecBuf->num);

        if (pRecBuf->num < 0 || pRecBuf->num >= numRecs ||
            strcmp(pRecBuf->str, stringBuf) ||
            pRecBuf->r != (float)pRecBuf->num) {
            printf("VerifyFile: invalid record = [%s, %d, %f]\n",
                   pRecBuf->str, pRecBuf->num, pRecBuf->r);
            exit(1);
        }

        if (found[pRecBuf->num]) {
            printf("VerifyFile: duplicate record = [%s, %d, %f]\n",
                   pRecBuf->str, pRecBuf->num, pRecBuf->r);
            exit(1);
        }

        found[pRecBuf->num] = 1;
    }

    if (rc != RM_EOF)
        goto err;

    if ((rc=fs.CloseScan()))
        return (rc);

    // make sure we had the right number of records in the file
    if (n != numRecs) {
        printf("%d records in file (supposed to be %d)\n",
               n, numRecs);
        exit(1);
    }
    // Return ok
    rc = 0;

err:
    fs.CloseScan();
    delete[] found;
    return (rc);
}

//
// PrintFile
//
// Desc: Print the contents of the file
//
RC PrintFile(RM_FileScan &fs)
{
    RC        rc;
    int       n;
    TestRec   *pRecBuf;
    RID       rid;
    RM_Record rec;

    printf("\nprinting file contents\n");

    // for each record in the file
    for (rc = GetNextRecScan(fs, rec), n = 0;
         rc == 0;
         rc = GetNextRecScan(fs, rec), n++) {

        // Get the record data and record id
        if ((rc = rec.GetData((char *&)pRecBuf)) ||
            (rc = rec.GetRid(rid)))
            return (rc);

        // Print the record contents
        PrintRecord(*pRecBuf);
    }

    if (rc != RM_EOF)
        return (rc);

    printf("%d records found\n", n);

    // Return ok
    return (0);
}

////////////////////////////////////////////////////////////////////////
// The following functions are wrappers for some of the RM component  //
// methods.  They give you an opportunity to add debugging statements //
// and/or set breakpoints when testing these methods.                 //
////////////////////////////////////////////////////////////////////////

//
// CreateFile
//
// Desc: call RM_Manager::CreateFile
//
RC CreateFile(char *fileName, int recordSize)
{
    printf("\ncreating %s\n", fileName);
    return (rmm.CreateFile(fileName, recordSize));
}

//
// DestroyFile
//
// Desc: call RM_Manager::DestroyFile
//
RC DestroyFile(char *fileName)
{
    printf("\ndestroying %s\n", fileName);
    return (rmm.DestroyFile(fileName));
}

//
// OpenFile
//
// Desc: call RM_Manager::OpenFile
//
RC OpenFile(char *fileName, RM_FileHandle &fh)
{
    printf("\nopening %s\n", fileName);
    return (rmm.OpenFile(fileName, fh));
}

//
// CloseFile
//
// Desc: call RM_Manager::CloseFile
//
RC CloseFile(char *fileName, RM_FileHandle &fh)
{
    if (fileName != NULL)
        printf("\nClosing %s\n", fileName);
    return (rmm.CloseFile(fh));
}

//
// InsertRec
//
// Desc: call RM_FileHandle::InsertRec
//
RC InsertRec(RM_FileHandle &fh, char *record, RID &rid)
{
    return (fh.InsertRec(record, rid));
}

//
// DeleteRec
//
// Desc: call RM_FileHandle::DeleteRec
//
RC DeleteRec(RM_FileHandle &fh, RID &rid)
{
    return (fh.DeleteRec(rid));
}

//
// UpdateRec
//
// Desc: call RM_FileHandle::UpdateRec
//
RC UpdateRec(RM_FileHandle &fh, RM_Record &rec)
{
    return (fh.UpdateRec(rec));
}

//
// GetNextRecScan
//
// Desc: call RM_FileScan::GetNextRec
//
RC GetNextRecScan(RM_FileScan &fs, RM_Record &rec)
{
    return (fs.GetNextRec(rec));
}
RC GetLastPositionOccupied(RM_FileHandle &fh, PageNum *pageNum, SlotNum *slotNum) {
	RM_Record rec;
	RM_FileScan	sc;
	RID rid;

	TRY(sc.OpenScan(fh, INT, sizeof(int), 0, NO_OP, NULL));
	
	*pageNum = 0, *slotNum = 0;

	for (RC rc; rc = sc.GetNextRec(rec), rc != RM_EOF;) {
		if (rc) {
			return rc;
		}
		TRY(rec.GetRid(rid));
		PageNum	x;
		SlotNum y;
		rid.GetPageNum(x);
		rid.GetSlotNum(y);
		if (*pageNum < x || (*pageNum == x && *slotNum < y)) {
			*pageNum = x;
			*slotNum = y;
		}
	}

	return 0;
}
/////////////////////////////////////////////////////////////////////
// Sample test functions follow.                                   //
/////////////////////////////////////////////////////////////////////

//
// Test1 tests simple creation, opening, closing, and deletion of files
//
RC Test1(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("test1 starting ****************\n");

    if ((rc = CreateFile((char *)FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile((char *)FILENAME, fh)) ||
        (rc = CloseFile((char *)FILENAME, fh)))
        return (rc);

    LsFile((char *)FILENAME);

    if ((rc = DestroyFile((char *)FILENAME)))
        return (rc);

    printf("\ntest1 done ********************\n");
    return (0);
}

//
// Test2 tests adding a few records to a file.
//
RC Test2(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("test2 starting ****************\n");

    if ((rc = CreateFile((char *)FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile((char *)FILENAME, fh)) ||
        (rc = AddRecs(fh, FEW_RECS)) ||
        (rc = VerifyFile(fh, FEW_RECS)) || 
        (rc = CloseFile((char *)FILENAME, fh)))
        return (rc);

    LsFile((char *)FILENAME);

    if ((rc = DestroyFile((char *)FILENAME)))
        return (rc);

    printf("\ntest2 done ********************\n");
    return (0);
}

// 
// Test3 tests adding a large number of records to a file.
//
RC Test3(void)
{
    RC  rc;
    RM_FileHandle fh;

    printf("test3 starting *******************\n");

    if ((rc = CreateFile((char *)FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile((char *)FILENAME, fh)) ||
        (rc = AddRecs(fh, LOTS_OF_RECS)) ||
        (rc = VerifyFile(fh, LOTS_OF_RECS)) ||
        (rc = CloseFile((char *)FILENAME, fh)))
            return (rc);

    LsFile((char *)FILENAME);

    if ((rc = DestroyFile((char *)FILENAME)))
            return (rc);

    printf("\ntest3 done *********************\n");
    return (0);
}

//
// Test4 tests scanning the record
//
RC Test4(void)
{
    RC rc;
    RM_FileHandle fh;

    printf("test4 starting *******************\n");

    if ((rc = CreateFile((char *)FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile((char *)FILENAME, fh)) ||
        (rc = AddRecs(fh, FEW_RECS)) ||
        (rc = VerifyFile(fh, FEW_RECS)))
        return (rc);

    RM_FileScan scan;
    int numComp = 10;
    printf("scanning records whose num <= %d\n", numComp);
    TRY(scan.OpenScan(fh, INT, sizeof(int), offsetof(TestRec, num), LT_OP, (void*)&numComp));
    //TRY(scan.OpenScan(fh, INT, sizeof(int), offsetof(TestRec, num), NE_OP, (void*)&numComp));
    //TRY(scan.OpenScan(fh, INT, sizeof(int), offsetof(TestRec, num), NO_OP, (void*)&numComp));
    //PrintFile(scan);
    {
        int rc, n = 0;
        RM_Record rec;
        while (1) {
            rc = scan.GetNextRec(rec);
            if (rc == RM_EOF) {
                break; 
            } else if (rc != 0) {
                return rc; 
            }
            ++n;
            char *data;
            rec.GetData(data);
            assert(((TestRec *)data)->num < numComp);
        }
        printf("%d records found.\n", n);
        assert(n == numComp);
    }

    if ((rc = CloseFile((char *)FILENAME, fh)) ||
        (rc = DestroyFile((char *)FILENAME)))
        return (rc);

    printf("\ntest4 done *********************\n");
    return (0);
}

//
// Test5 tests updating some records
RC Test5(void) 
{
   RC   rc;
   RM_FileHandle fh;

   printf("test5 starting *******************\n");


   if ((rc = CreateFile((char *)FILENAME, sizeof(TestRec))) ||
       (rc = OpenFile((char *)FILENAME, fh)) ||
       (rc = AddRecs(fh, FEW_RECS)) ||
       (rc = VerifyFile(fh, FEW_RECS)))
        return rc;
   
   RM_Record rec;
   RM_FileScan scan;
  
   TRY(scan.OpenScan(fh, INT, sizeof(int), 0, NO_OP, NULL));
   for (rc = scan.GetNextRec(rec); rc != RM_EOF; rc = scan.GetNextRec(rec)) {
        if (rc) {
            return rc; 
        } 
        TestRec *data;
        rec.GetData(CVOID(data));
        data->num++;
        TRY(fh.UpdateRec(rec));
   }
   TRY(scan.CloseScan());

   TRY(scan.OpenScan(fh, INT, sizeof(int), 0, NO_OP, NULL));
   for (rc = scan.GetNextRec(rec); rc != RM_EOF; rc = scan.GetNextRec(rec)) {
        if (rc) {
            return rc; 
        } 
        TestRec *data;
        rec.GetData(CVOID(data));
        int old_num; 
        sscanf(data->str, "a%d", &old_num);
        assert(old_num + 1 == data->num); 
   }
   TRY(scan.CloseScan());

   if ((rc = CloseFile((char *)FILENAME, fh)) ||
       (rc = DestroyFile((char *)FILENAME)))
        return rc;

   printf("\ntest5 done ***********************\n");
   return 0;
}

//
// Test6 tests deleting 
RC Test6(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("test6 starting ****************\n");

	int m = 100;

    if ((rc = CreateFile((char *)FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile((char *)FILENAME, fh)) ||
        (rc = AddRecs(fh, m)) ||
        (rc = VerifyFile(fh, m)))
        return (rc);

    RM_Record rec;
    RM_FileScan sc;

    char searchStr[] = {"a8"};
	RID rid;

	TRY(sc.OpenScan(fh, STRING, strlen(searchStr), offsetof(TestRec, str), EQ_OP, searchStr));
	while (rc = sc.GetNextRec(rec), rc != RM_EOF) {
		if (rc) {
			return rc;	
		}
		TestRec* data;
		rec.GetData(CVOID(data));
		PrintRecord(*data);
		TRY(rec.GetRid(rid));	
		TRY(fh.DeleteRec(rid));
	} 
	TRY(sc.CloseScan());
	
	//TRY(sc.OpenScan(fh, STRING, strlen(searchStr), offsetof(TestRec, str),
     //          EQ_OP, searchStr));
    //TRY(sc.GetNextRec(rec));
    //assert(sc.GetNextRec(rec) == RM_EOF);
    //TRY(sc.CloseScan());

    //TRY(rec.GetRid(rid));
    //TRY(fh.DeleteRec(rid));

    TRY(sc.OpenScan(fh, STRING, strlen(searchStr), offsetof(TestRec, str),
            EQ_OP, searchStr));
    assert(sc.GetNextRec(rec) == RM_EOF);
    TRY(sc.CloseScan());

    if ((rc = CloseFile((char *)FILENAME, fh)) ||
        (rc = DestroyFile((char *)FILENAME)))
        return (rc);

    printf("\ntest6 done ********************\n");
    return (0);
}

//
// Test7 tests reusing space of deleted records
//
RC Test7(void) {
	RC	rc;
	RM_FileHandle fh;

	printf("test7 starting******************\n");
	
	int recsPerPage = 99; // calculated records per page; this might change
	int pages = 5;
	int recsToDel = 100;

	int n = recsPerPage * pages;

	printf("Insert records of %d pages, total %d\n", pages, n);
	if ((rc = CreateFile((char *)FILENAME, sizeof(TestRec))) ||
		(rc = OpenFile((char *)FILENAME, fh)) ||
		(rc = AddRecs(fh, n)) ||
		(rc = VerifyFile(fh, n))) {
		return rc;	
	}

	PageNum x;
	SlotNum y;

	GetLastPositionOccupied(fh, &x, &y);
	printf("Last position occupied = ( %d , %d)\n", x, y);
	assert(x == pages);

	RM_Record rec;
	RM_FileScan sc;
	RID rid;

	printf("Delete first %d records\n", recsToDel);
	TRY(sc.OpenScan(fh, INT, sizeof(INT), offsetof(TestRec, num), LT_OP, &recsToDel));
	int count = 0;
	for (RC rc; rc = sc.GetNextRec(rec), rc != RM_EOF;) {
		if (rc) {
			return rc;
		}
		TRY(rec.GetRid(rid));
		TRY(fh.DeleteRec(rid));
		++count;
	}
	assert(count == recsToDel);
	TRY(sc.CloseScan());
	
	GetLastPositionOccupied(fh, &x, &y);
	printf("Last position occupied = (%d , %d)\n", x, y);
	assert(x == pages);

	printf("Insert another %d records\n", recsToDel);
	for (int i = 0; i < recsToDel; ++i) {
		TestRec tr;
		memset(tr.str, 0, sizeof(tr.str));
		sprintf(tr.str, "n%d", i);
		tr.num = i;
		tr.r = (float)i;
		TRY(fh.InsertRec((char *)&tr, rid));	
	}

	// the page used should not change
	GetLastPositionOccupied(fh, &x, &y);
	printf("Last position occupied = (%d, %d)\n", x, y);
	assert(x == pages);
	
	int firstIndexDeleted = n - recsToDel;

	printf("Delete last %d records\n", recsToDel);
	TRY(sc.OpenScan(fh, INT, sizeof(int), offsetof(TestRec, num), GE_OP, &firstIndexDeleted));
	count = 0;
	for (RC rc; rc = sc.GetNextRec(rec), rc != RM_EOF; ) {
		if (rc) {
			return rc;
		}
		TRY(rec.GetRid(rid));
		TRY(fh.DeleteRec(rid));
		++count;
	}
	assert(count == recsToDel);
	TRY(sc.CloseScan());

	GetLastPositionOccupied(fh, &x, &y);
	printf("Last position occupied = (%d, %d)\n", x, y);
	printf("Insert another %d records\n", recsToDel);
	for (int i = 0; i < recsToDel; i++) {
		TestRec tr;
		memset(tr.str, 0, sizeof(tr.str));
		sprintf(tr.str, "m%d", i);
		tr.num = i;
		tr.r = (float)i;
		TRY(fh.InsertRec((char *)&tr, rid));
	}
	
	// the page used should not change
	GetLastPositionOccupied(fh, &x, &y);
	printf("Last position occupied = (%d, %d)\n", x, y);
	assert(x == pages);
	
	if ((rc = CloseFile((char *)FILENAME, fh)) ||
		(rc = DestroyFile((char *)FILENAME))) 
		return rc;

	printf("test7 done *****************\n");
	return 0;
}
