#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <random>
#include <cmath>
#include <fstream>
#include <string>
#include <iomanip>
#include <algorithm>

using namespace std;
//default_random_engine e((unsigned  long) time(nullptr));                           //don't use it anymore,use the following
random_device rd;// 定义一个随机数引擎,
mt19937 e(rd());
uniform_real_distribution<double> u(0, 1);

// for data generation
// change NumRA , RestA_info, Truck_info1.csv, RestArea1.csv

const double L = 2000.0;                       //Total simulation distance unit in mile
const int NumRA = 7;                                  //number of rest area
const int MaxCy = 24;                          // max hours run in the simulation,see input file, can be 48...
const int TT = MaxCy*2;                     // total simulation truck number(entering)
const int TT2 = MaxCy*2;                     // total simulation truck number(exit)

//Three modes: driving, searching for parking, parking

//To do
// Occupied rate( consider capacity)
// Documentation
// Header file

struct TruckPropStru
{
    double speed;               //Driving speed
    double DRbefore;            //Driving time before entering the highway
    double StartT;              //Time entering the highway

    vector<double> BP;          //Time when driver decides to take rest
    vector<int> RN;             // Rest area number for trucker's rest
    vector<double> RestTime;    // Duration of break

    //consider combine BP1, BP2 to BP[i]
    //consider combine RestShort, RestLong to Rest[i]
    //consider combine RS,RL to R[i]
    //then re-enter and multi resting is possible
    // end condition: truck driving out of observing segment
    //
    vector<double> Entryd;              //entry distance (distance to point 0) default value =0
    vector<double> Exitd;              //exit distance (distance to point 0) default value =L
};

struct RestAreaStru
{
    int id;             // Rest area's ID , stands for the distance from the observation point
    double location;    //rest area location
    int Snum[MaxCy*5];       // number of trucks taking SHORT rest
    // eg. Snum[3] store the number of trucks parked at RestArea from [3,4)
    // if truck parks from 1.5 to 3.5, then Sunm[1],[2],[3] all plus 1.
    int Lnum[MaxCy*5];       //number of trucks taking LONG rest

};

struct RegulationStru
{
    double MaxWS;   //Maximum working hours until interruption by short break regulation
    double MaxWL;    //Maximum working hours until interruption by long break regulation

};

struct TruckFlowStru
{
    double ArrTime;    // arrival time , begin
    int ML;         // number of truck in the hour
    double EnterLoc;    // entrance location
};
//partical OD

struct EnterExitStru
{
    double Etd;     //enterance distance to point 0
    double Exd;     //exit distance to point 0
    int num;        //number of trucks entering or exiting
};

int PreferS(int farest)        // preference,return the preferred parking number, can summerize from data
// consider service and legal driving hour left by far
{
    int temp = farest;                      // store the preferred restarea in the iteration
    double Score[100] = {6};      //score of each rest area
    for(int num = 1; num < farest; num++)
    {
        if(Score[num] > Score[farest]) {
            temp = num;
        }
    }
    return temp;
}

int PreferL(int local, int farest)        // preference,return the preferred parking number, can summerize from data
// truck just drive out from rest area local
// consider service and legal driving hour left by far
{
    int temp = farest;                      // store the preferred restarea in the iteration
    double Score[100] = {6};      //Score[m] m value is the same, score of each rest area
    for(int num = local + 1; num < farest; num++)
    {
        if(Score[num] > Score[farest]) {
            temp = num;
        }
    }
    return temp;
}
//Calculate the location between each rest area, from rest area i to rest are j
double DistRR(int i,int j,struct RestAreaStru RestArea[])
{
    if(j > i ){
        return RestArea[j].location - RestArea[i].location;
    }else {
        return 0.0001;
    }
}

double DistER(double i,int j,struct RestAreaStru RestArea[])
{
    if(RestArea[j].location > i ){
        return RestArea[j].location - i;
    }else {
        return 0;
    }
}

double DrivingTime(double a)
{
    if(a < 0 ){
        DrivingTime(abs(a)) ;
    }else if(a > 7.99){
        DrivingTime(abs(a - 7.99));
    }else if(a < 2){
        return 2;
    }else{
        return a;
    };
}

/*################################  Truck2Rest Function   ##################################*/

// The function works for 24 hour

// To do:
// add capacity consideration, violation,etc
// add perference of rest area
// simulate circular of segment of highway?

//
//################################       Enter   function    ##################################
void Truck2RestC(struct TruckPropStru *Truck, struct RestAreaStru RestArea[],vector<double> &REE,int m)
{
    //default_random_engine e((unsigned  long) time(NULL));// 定义一个随机数引擎
    std::random_device rd;
    std::mt19937 e(rd());
    //https://gaomf.cn/2017/03/22/C++_Random/

    normal_distribution<double> nor1(7.5,0.5);   //Normal distribution, use for first driving time 7.5,15
    normal_distribution<double> nor2(2.5,0.5);   //Normal distribution, use for second driving time 2.5,15

    //https://homepage.divms.uiowa.edu/~mbognar/applets/normal.html
    m = NumRA;
    int it = m - 1;                  //iterator
    int a = 0;                       // store the RestArea number of SHORT rest
    int b = 0;                       // store the RestArea number of LONG rest
    int s1 = 0;                      // store the entering time of SHORT and LONG rest
    int s2 = 0;                      // store the leaving time number of SHORT LONG rest, in 1 hour interval
    int cir = 0;                        // number of loop the truck drive
    double rem = 0;                     // remaining distance after deduction circle
    double legal = 0;                   // record legal driving time
    RegulationStru Reg = {8.0,11.0};     // USDOT HOS Regulation

    cout<<"m1 = "<<m<<endl;

    if(Truck->DRbefore < Reg.MaxWS) {
        legal = min((Reg.MaxWS - Truck->DRbefore ), DrivingTime(nor1(e)));// nor1(e) is the first part driving time
        Truck->BP.push_back(Truck->StartT + legal);
        // allocate the rest area to decide the truck BP1
        // truck cannot park randomly, a is actually the last RestArea number
        // the driver can park in order to obey HOS
        // find the parking for short rest

        cout<<"Truck->speed * legal "<<Truck->speed * legal<<endl;

/*
        for(int i = 0; i < NumRA; i++){
            if(DistER(Truck->Entryd.back(), i ,RestArea) == 0){
                it = i  -1;
            }
        }
*/

        //if the truck drives out of the segment, drop the data point
        if((Truck->speed * legal) >  DistER(Truck->Entryd.back(),m-1,RestArea)){
            Truck->BP.clear();//error code for the truck, the data point will be dropped
            Truck->RN.clear();
            Truck->RestTime.clear();
            Truck->Entryd.clear();
            Truck->Exitd.clear();
            return;
        }

        while (((Truck->speed * legal) < DistER(Truck->Entryd.back(),it,RestArea)))
        {
            it = it  -1;
        };
        if((DistER(Truck->Entryd.back(),it,RestArea)) == 0){
            it = it + 1;
        }//might be problem

        a = it;
        cout << "a = "<<a << endl;

        //consider preference here or another function in the header file
        //the driver can park at the place he prefer in [0,a], choose the preferred RestArea
        //if the variance of the preference is below threshold, then choose the farest RestArea (a)
        //update BP1 value
        //similar to BP1
        //a is nearest RestArea

        Truck->RN.push_back(PreferS(a));     //rest area for short rest
        Truck->BP.back() = Truck->StartT + (DistER(Truck->Entryd.back(),a,RestArea)) / Truck->speed;
        // short rest time
        s1 = int(floor(Truck->BP.back())) ;//Time truck enters the RestArea[a], round down
        s2 = int(ceil(Truck->BP.back() + Truck->RestTime.at(Truck->RestTime.size()-2)));//Time the truck leave the rest area, round up

        //number of short term parking statistics
        while ((s1 < s2) || (s1 == s2)) {
            RestArea[a].Snum[s1] = RestArea[a].Snum[s1] + 1;
            s1++;
        }

        //for next rest part(long rest) ####################
        Truck->BP.push_back(Truck->BP.back() + Truck->RestTime.at(Truck->RestTime.size()-2) + min((Reg.MaxWL+ Truck->StartT - Truck->BP.back()), DrivingTime(nor2(e))));
        // the latter is the same as driving time function in the first part
        // truck driver driving time can be less than the regulation

        it = a + 1; //reset it value; if a < m -1 (19), then try from next rest area, which is a +1;
        //else when a = m -1, try from the first rest area, which is 0

        rem = ((Truck->BP.back() - Truck->BP.at(Truck->BP.size()-2)  - Truck->RestTime.at(Truck->RestTime.size()-2))* Truck->speed);
        //change here tomorrow, to lower and upper limit
        if(rem >  DistRR(a,m -1,RestArea)){//here it = m-1
            //Truck->BP.push_back(88);//error code for the truck, the data point will be dropped
            return;
        }

        while(rem > DistRR(a,it,RestArea))
        {//test whether the truck has left the segme
            it = it + 1 ;
            //cout<<rem<<" " <<DistRR(a,it,RestArea)<<endl;
        }

    }else{// when the truck only have long rest in the observed section
        legal = min((Reg.MaxWL - Truck->DRbefore ), DrivingTime(nor1(e)));// nor1(e) is the first part driving time
        Truck->BP.push_back(Truck->StartT);

        Truck->RestTime.at(Truck->RestTime.size()-2) = 0;
        Truck->BP.push_back(Truck->StartT + legal);

        a = 0;// code for only consider the second driving
        Truck->RN.push_back(a);// code for only observing the second part

        rem = ((Truck->BP.back() - Truck->BP.at(Truck->BP.size()-2)  - Truck->RestTime.at(Truck->RestTime.size()-2))* Truck->speed);
        //change here tomorrow, to lower and upper limit
        if(rem >  DistER(Truck->Entryd.back(),m -1,RestArea)){//here it = m-1 89757
            //Truck->BP.push_back(88);//error code for the truck, the data point will be dropped
            b= 77; // code for second part beyond highway limit
            Truck->RN.push_back(b);
            return;
        }
        else{
            it = 0;
            while(int(floor(Truck->Entryd.back()/ RestArea[it].location)) != 0)
            {//test whether the truck has left the segment
                it = it + 1;
            }
        }

        while(rem > DistER(Truck->Entryd.back(),it,RestArea))
        {//test whether the truck has left the segme
            it = it + 1 ;
            cout<<rem<<" " <<DistRR(a,it,RestArea)<<endl;
        }
    }

    if(it == 0){
        b = m -1;
    }else if(it == (a + 1) % m) {
        b = a;
    }else{
        b = it -1;
    }

    cout<<"b = "<<b<<endl;
    Truck->RN.push_back(b);

    Truck->BP.back() = Truck->BP.at(Truck->BP.size()-2) + Truck->RestTime.at(Truck->RestTime.size()-2)  + DistRR(a,b,RestArea) / Truck->speed;


    s1 = int(floor(Truck->BP.back()));//Time truck enters the RestArea[b], round down
    s2 = int(ceil(Truck->BP.back() + Truck->RestTime.back()));//Time truck leaves the RestArea[b]

    while((s1 < s2) || (s1 == s2) ) {
        RestArea[b].Lnum[s1] = RestArea[b].Lnum[s1] + 1;
        s1 ++;
        //consider change Lnum to vector;!!!!!!!!!!!!!!!!1
    }


    REE.push_back(Truck->BP.back() + Truck->RestTime.back());  //save leaving time (re-enter after long rest)###############################################
}

//################################       Exit   Function     ##################################
void Truck2RestEx(struct TruckPropStru *Truck, struct RestAreaStru RestArea[],vector<double> &REE,int m)
{
    //default_random_engine e((unsigned  long) time(NULL));// 定义一个随机数引擎
    std::random_device rd;
    std::mt19937 e(rd());
    //https://gaomf.cn/2017/03/22/C++_Random/

    normal_distribution<double> nor1(7.5,0.5);   //Normal distribution, use for first driving time 7.5,15
    normal_distribution<double> nor2(2.5,0.5);   //Normal distribution, use for second driving time 2.5,15

    //https://homepage.divms.uiowa.edu/~mbognar/applets/normal.html
    m = NumRA;
    int it = m - 1;                  //iterator
    int a = 0;                       // store the RestArea number of SHORT rest
    int b = 0;                       // store the RestArea number of LONG rest
    int s1 = 0;                      // store the entering time of SHORT and LONG rest
    int s2 = 0;                      // store the leaving time number of SHORT LONG rest, in 1 hour interval
    double rem = 0;                     // remaining distance after deduction circle
    double legal = 0;                   // record legal driving time
    RegulationStru Reg = {8.0,11.0};     // USDOT HOS Regulation

    cout<<"m1 = "<<m<<endl;

    if(Truck->DRbefore < Reg.MaxWS) {
        legal = min((Reg.MaxWS - Truck->DRbefore ), DrivingTime(nor1(e)));// nor1(e) is the first part driving time
        Truck->BP.push_back(Truck->StartT + legal);
        // allocate the rest area to decide the truck BP1
        // truck cannot park randomly, a is actually the last RestArea number
        // the driver can park in order to obey HOS
        // find the parking for short rest

        cout<<"Truck->speed * legal "<<Truck->speed * legal<<endl;

/*
        for(int i = 0; i < NumRA; i++){
            if(DistER(Truck->Entryd.back(), i ,RestArea) == 0){
                it = i  -1;
            }
        }
*/

        //if the truck drives out of the segment, drop the data point
        if((Truck->speed * legal) >  DistER(Truck->Entryd.back(),m-1,RestArea)){
            Truck->BP.clear();//error code for the truck, the data point will be dropped
            Truck->RN.clear();
            Truck->RestTime.clear();
            Truck->Entryd.clear();
            Truck->Exitd.clear();
            return;
        }

        while (((Truck->speed * legal) < DistER(Truck->Entryd.back(),it,RestArea)))
        {
            it = it  -1;
        };

        if((DistER(Truck->Entryd.back(),it,RestArea)) == 0){
            it = it + 1;
        }//might be problem
        a = it;
        cout << "a = "<<a << endl;

        //consider preference here or another function in the header file
        //the driver can park at the place he prefer in [0,a], choose the preferred RestArea
        //if the variance of the preference is below threshold, then choose the farest RestArea (a)
        //update BP1 value
        //similar to BP1
        //a is nearest RestArea

        Truck->RN.push_back(PreferS(a));     //rest area for short rest
        Truck->BP.back() = Truck->StartT + (DistER(Truck->Entryd.back(),a,RestArea)) / Truck->speed;
        // short rest time
        s1 = int(floor(Truck->BP.back())) ;//Time truck enters the RestArea[a], round down
        s2 = int(ceil(Truck->BP.back() + Truck->RestTime.at(Truck->RestTime.size()-2)));//Time the truck leave the rest area, round up

        //number of short term parking statistics
        while ((s1 < s2) || (s1 == s2)) {
            RestArea[a].Snum[s1] = RestArea[a].Snum[s1] - 1;
            s1++;
        }

        //for next rest part(long rest) ####################
        Truck->BP.push_back(Truck->BP.back() + Truck->RestTime.at(Truck->RestTime.size()-2) + min((Reg.MaxWL+ Truck->StartT - Truck->BP.back()), DrivingTime(nor2(e))));
        // the latter is the same as driving time function in the first part
        // truck driver driving time can be less than the regulation

        it = a + 1; //reset it value; if a < m -1 (19), then try from next rest area, which is a +1;
        //else when a = m -1, try from the first rest area, which is 0

        rem = ((Truck->BP.back() - Truck->BP.at(Truck->BP.size()-2)  - Truck->RestTime.at(Truck->RestTime.size()-2))* Truck->speed);
        //change here tomorrow, to lower and upper limit
        if(rem >  DistRR(a,m -1,RestArea)){//here it = m-1
            //Truck->BP.push_back(88);//error code for the truck, the data point will be dropped
            return;
        }

        while(rem > DistRR(a,it,RestArea))
        {//test whether the truck has left the segme
            it = it + 1 ;
            //cout<<rem<<" " <<DistRR(a,it,RestArea)<<endl;
        }

    }else{// when the truck only have long rest in the observed section
        legal = min((Reg.MaxWL - Truck->DRbefore ), DrivingTime(nor1(e)));// nor1(e) is the first part driving time
        Truck->BP.push_back(Truck->StartT);

        Truck->RestTime.at(Truck->RestTime.size()-2) = 0;
        Truck->BP.push_back(Truck->StartT + legal);

        a = 66;// code for only consider the second driving
        Truck->RN.push_back(a);// code for only observing the second part

        rem = ((Truck->BP.back() - Truck->BP.at(Truck->BP.size()-2)  - Truck->RestTime.at(Truck->RestTime.size()-2))* Truck->speed);
        //change here tomorrow, to lower and upper limit
        if(rem >  DistER(Truck->Entryd.back(),m -1,RestArea)){//here it = m-1 89757
            //Truck->BP.push_back(88);//error code for the truck, the data point will be dropped
            b= 77; // code for second part beyond highway limit
            Truck->RN.push_back(b);
            return;
        }
        else{
            it = 0;
            while(int(floor(Truck->Entryd.back()/ RestArea[it].location)) != 0)
            {//test whether the truck has left the segment
                it = it + 1;
            }
        }

        while(rem > DistER(Truck->Entryd.back(),it,RestArea))
        {//test whether the truck has left the segme
            it = it + 1 ;
            cout<<rem<<" " <<DistRR(a,it,RestArea)<<endl;
        }
    }

    if(it == 0){
        b = m -1;
    }else if(it == (a + 1) % m) {
        b = a;
    }else{
        b = it -1;
    }

    cout<<"b = "<<b<<endl;
    Truck->RN.push_back(b);


    Truck->BP.back() = Truck->BP.at(Truck->BP.size()-2) + Truck->RestTime.at(Truck->RestTime.size()-2)  + DistRR(a,b,RestArea) / Truck->speed;


    s1 = int(floor(Truck->BP.back()));//Time truck enters the RestArea[b], round down
    s2 = int(ceil(Truck->BP.back() + Truck->RestTime.back()));//Time truck leaves the RestArea[b]

    while((s1 < s2) || (s1 == s2) ) {
        RestArea[b].Lnum[s1] = RestArea[b].Lnum[s1] - 1;
        s1 ++;
        //consider change Lnum to vector;!!!!!!!!!!!!!!!!1
    }


    REE.push_back(Truck->BP.back() + Truck->RestTime.back());  //save leaving time (re-enter after long rest)###############################################
}





/*################################  Main Function   ##################################*/
int main() {
    ofstream outFile;
    //generate random numbers
    uniform_real_distribution<double> u(0, 1);          // 定义一个范围为0~1的浮点数分布类型
    random_device rd;// 定义一个随机数引擎
    mt19937 e(rd());
    //std::normal_distribution<double> distribution(5.0,2.0);   //normal distribution
    lognormal_distribution<double> lgn2(0.1,0.1);  // Log-normal distribution,use for short rest time //default lgn2(0.1,0.1)
    //lognormal_distribution<double> lgn3(2,0.1);
    //normal_distribution<double> nort1(11,0.5);

    //normal_distribution<double> nor1()
    //https://www.medcalc.org/manual/log-normal_distribution_functions.php
    //https://homepage.divms.uiowa.edu/~mbognar/applets/normal.html

    /*Parameters*/
    int i = 0;                               // Iterate for Truck combined with n
    int j = 0;                               // Iterate for RestArea combined with m
    int l = 0;                               // Random Iterator
    int count = 0;                      // for the count, run times of truck
    int n = 0;                           //number of trucks to simulate entering from point 0
    int tn = n;                              // total number of trucks to simulate
    int m = NumRA;                              // number of rest area
    //int et = 21;                             // total number of combination of entrance and exit
    // (assume 5 entr , 5 exit, plus initial entry and final exit) C7_2 = 7x6/(2x1)=21
    int t = 0;                               // time point for print out
    vector<double> LDH = {};                 //Legal driving time
    vector<double> REE = {};                 //save leaving time (re-enter point for each truck after long rest)
    //initialization

    RestAreaStru RestArea[m] = {0,0.0,{0},{0}};

    ifstream infile_r("RestA_info502.txt",ios::in);
    if(!infile_r)
    {
        cout << "Error: opening RestArea file fail" << endl;
        exit(1);
    }
    while(!infile_r.eof() && j < m)
    {
        infile_r >> RestArea[j].id >> RestArea[j].location;

        //read previous rest truck
//        for( i = 0 ; i< 24 ; i++)
//        {
//            infile_r >> RestArea[j].Snum[i] ;
//        }
//        for( i = 0 ; i< 24 ; i++)
//        {
//            infile_r >>  RestArea[j].Lnum[i];
//        }
        j++;

    }

    infile_r.close();
//##################################### Enter ###############################################
    //arrival time for each truck
    TruckFlowStru TruckFlow[TT];

    ifstream infile_t("Truck_arr.txt",ios::in);
    if(!infile_t)
    {
        cout << "Error: opening RestArea file fail" << endl;
        exit(1);
    }

    j = 0;

    while(!infile_t.eof() && j < TT)
    {
        infile_t >> TruckFlow[j].ArrTime >> TruckFlow[j].ML>>TruckFlow[j].EnterLoc;
        j++;
    }

    infile_t.close();
    cout<<"here"<<endl;

    vector<double> arrival;
    vector<double> startpoint;

    for ( i = 0 ; i < TT; i++)
    {
        n = n + TruckFlow[i].ML ;// n is total number of trucks
    }
    cout<<"n ="<<n<<endl;
    cout<<TruckFlow[0].ML<<endl;

    for(i = 0; i< TT; i++)
    {
        for(j = 0; j< TruckFlow[i].ML; j++)
        {
            arrival.push_back((TruckFlow[i].ArrTime + u(e)));
            startpoint.push_back((TruckFlow[i].EnterLoc));
        }
    }
    for(i =0; i< n; i++)//delete later
    {
        cout<<arrival.at(i)<<endl;
        cout<<startpoint.at(i)<<endl;
    }

//#####################################  Enter input is over  ############################################
        // ############################################# Exit ###########################################
    TruckFlowStru TruckFlow2[TT2];//all 2 is exit

    ifstream infile_ex("Truck_ex.txt",ios::in);
    if(!infile_ex)
    {
        cout << "Error: Truck_ex file fail" << endl;
        exit(1);
    }

    j = 0;

    while(!infile_ex.eof() && j < TT2)
    {
        infile_ex >> TruckFlow2[j].ArrTime >> TruckFlow2[j].ML>>TruckFlow2[j].EnterLoc;
        j++;
    }

    infile_ex.close();
    cout<<"here"<<endl;

    vector<double> arrival2;
    vector<double> startpoint2;

    int n2 = 0;

    for ( i = 0 ; i < TT2; i++)
    {
        n2 = n2 + TruckFlow2[i].ML ;// n is total number of trucks
    }

    for(i = 0; i< TT2; i++)
    {
        for(j = 0; j< TruckFlow2[i].ML; j++)
        {
            arrival2.push_back((TruckFlow2[i].ArrTime + u(e)));
            startpoint2.push_back((TruckFlow2[i].EnterLoc));
        }
    }
    for(i =0; i< n2; i++)//delete later
    {
        cout<<arrival2.at(i)<<endl;
        cout<<startpoint2.at(i)<<endl;
    }
//#####################################  Exit input is over  ############################################


    outFile.open("Truck_info503.csv",ios::out);
    outFile << std::setprecision(2) << std::fixed; // keep two decimals
    //print title
    outFile <<"TruckNum"<<","<<"StartT"<<","\
            <<"BP1"<<","<<"ShortRest"<<","<<"FirstParking"<<","\
            <<"BP2"<<","<<"LongRest"<<","<<"SecondParking"<<","\
            <<"Entry"<<","<<"Exit"<<"\n";

    TruckPropStru Truck;

    // trucks start from the entering point of highway and drive through the whole section
    for ( i = 0 ; i < n; i++)
    {
        //initialization
        m = NumRA;
        Truck.speed = 65;  //assume speed is 65 mph
        //################################  Set distributions   ##################################
        Truck.DRbefore = 9*u(e);// Driving time before entering the highway
        Truck.StartT = arrival.at(i); //Arrival function at entrance. In the future it can be replaced by traffic flow function
        // short and long rest time
        Truck.RestTime.push_back(lgn2(e));// rest time distribution truck leaves the RestArea[a], round up default lgn2(e)
        Truck.RestTime.push_back(5*u(e)+10);//5*u(e)+10 ( at least 10 hour rest)
        Truck.Entryd.push_back(startpoint.at(i));
        outFile <<i<<","<< Truck.StartT <<",";

        Truck2RestC(&Truck, RestArea,REE,m);//function
        Truck.Exitd.push_back(RestArea[Truck.RN.back()].location);
        cout<<"############################## n = "<<i <<endl;

        for (unsigned it = 0; it < Truck.BP.size(); it++)
        {
            outFile<<Truck.BP.at(it)<<","<< Truck.RestTime.at(it)<<","<<Truck.RN.at(it)<<",";
            if(it %2 == 1) {
                outFile << Truck.Entryd.at((it-1)/2) << "," << Truck.Exitd.at((it-1)/2)<<",";
            }
        }
        outFile<<endl;


        //clear and reset for next loop
        Truck.BP.clear();          //Time when driver decides to take rest
        Truck.RN.clear();             // Rest area number for trucker's rest
        Truck.RestTime.clear();    // Duration of break
        Truck.Entryd.clear();
        Truck.Exitd.clear();
        REE.clear();
    }


//############################################# Enter is over ###########################################

//############################################# Exit  #############################################

    // trucks start from the entering point of highway and drive through the whole section
    for ( i = 0 ; i < n2; i++)
    {
        //initialization
        m = NumRA;
        Truck.speed = 65;  //assume speed is 65 mph
        //################################  Set distributions   ##################################
        Truck.DRbefore = 9*u(e) ;// Driving time before entering the highway
        Truck.StartT = arrival2.at(i); //Arrival function at entrance. In the future it can be replaced by traffic flow function
        // short and long rest time
        Truck.RestTime.push_back(lgn2(e));// rest time distribution truck leaves the RestArea[a], round up default lgn2(e)
        Truck.RestTime.push_back(5*u(e)+10);//5*u(e)+10 ( at least 10 hour rest)
        Truck.Entryd.push_back(startpoint2.at(i));
        outFile <<i<<","<< Truck.StartT <<",";

        Truck2RestEx(&Truck, RestArea,REE,m);//function
        Truck.Exitd.push_back(RestArea[Truck.RN.back()].location);
        cout<<"############################## n = "<<n <<endl;

        for (unsigned it = 0; it < Truck.BP.size(); it++)
        {
            outFile<<Truck.BP.at(it)<<","<< Truck.RestTime.at(it)<<","<<Truck.RN.at(it)<<",";
            if(it %2 == 1) {
                outFile << Truck.Entryd.at((it-1)/2) << "," << Truck.Exitd.at((it-1)/2)<<",";
            }
        }
        outFile<<endl;


        //clear and reset for next loop
        Truck.BP.clear();          //Time when driver decides to take rest
        Truck.RN.clear();             // Rest area number for trucker's rest
        Truck.RestTime.clear();    // Duration of break
        Truck.Entryd.clear();
        Truck.Exitd.clear();
        REE.clear();
        count = 0;
    }


//############################################# Exit is over #############################################

    outFile.close();//close the truck csv


    //output RestArea
    outFile.open("RestArea504.csv");
    outFile << " Number of trucks in short rest \n" <<endl;
    outFile << "RestArea"<<","<<"Time"<<","<<"Number of trucks"<<endl;

    for( j = 0; j < m ; j++)
    {
        outFile <<"RestArea"<< j<<"\n";
        outFile<<"Time ";
        for ( t = 0; t <MaxCy*5; t++)//note!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        {
            outFile <<t<<"," ;
            if(t % 24 == 0 && t !=0){
                outFile << " day"<< int(floor(t/24))<<",";
            }
            //cout << t << " Number of trucks in long rest in rest area " << j << " is " << RestArea[j].Lnum[t] << endl;
        }
        outFile <<endl;
        outFile<<"Snum ";
        for ( t = 0; t <MaxCy*5; t++)//note!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        {
            outFile <<RestArea[j].Snum[t]<<"," ;
            if(t % 24 == 0 && t !=0){
                outFile << " day"<< int(floor(t/24))<<",";
            }
            //cout << t << " Number of trucks in long rest in rest area " << j << " is " << RestArea[j].Lnum[t] << endl;
        }
        outFile <<endl;

        outFile<<"Lnum ";
        for ( t = 0; t <MaxCy*5; t++)
        {
            outFile <<RestArea[j].Lnum[t]<<"," ;
            if(t % 24 == 0 && t !=0){
                outFile << " day "<< int(floor(t/24))<<",";
            }
            //cout << t << " Number of trucks in long rest in rest area " << j << " is " << RestArea[j].Lnum[t] << endl;
        }
        outFile <<endl;
    }


    outFile.close();


    return 0;
}

