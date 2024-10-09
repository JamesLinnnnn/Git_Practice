
/*=======================================================================
  A simple parser for "self" format

  The circuit format (called "self" format) is based on outputs of
  a ISCAS 85 format translator written by Dr. Sandeep Gupta.
  The format uses only integers to represent circuit information.
  The format is as follows:

1        2        3        4           5           6 ...
------   -------  -------  ---------   --------    --------
0 GATE   outline  0 IPT    #_of_fout   #_of_fin    inlines
                  1 BRCH
                  2 XOR(currently not implemented)
                  3 OR
                  4 NOR
                  5 NOT
                  6 NAND
                  7 AND

1 PI     outline  0        #_of_fout   0

2 FB     outline  1 BRCH   inline

3 PO     outline  2 - 7    0           #_of_fin    inlines

    The code was initially implemented by Chihang Chen in 1994 in C, 
    and was later changed to C++ in 2022 for the course porject of 
    EE658: Diagnosis and Design of Reliable Digital Systems at USC. 

=======================================================================*/

/*=======================================================================
    Guide for students: 
        Write your program as a subroutine under main().
        The following is an example to add another command 'lev' under main()

enum e_com {READ, PC, HELP, QUIT, LEV};
#define NUMFUNCS 5
void cread(), pc(), quit(), lev();
struct cmdstruc command[NUMFUNCS] = {
    {"READ", cread, EXEC},
    {"PC", pc, CKTLD},
    {"HELP", help, EXEC},
    {"QUIT", quit, EXEC},
    {"LEV", lev, CKTLD},
};

lev()
{
   ...
}
=======================================================================*/

#include <stdio.h>
#include <iostream>
#include <string>
#include <string.h>
#include <ctype.h>
#include <cstring>
#include <stdlib.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

#define MAXLINE 10000               /* Input buffer size */
#define MAXNAME 10000               /* File name size */

#define Upcase(x) ((isalpha(x) && islower(x))? toupper(x) : (x))
#define Lowcase(x) ((isalpha(x) && isupper(x))? tolower(x) : (x))

enum e_com {READ, PC, HELP, QUIT,LEV,LOGICSIM};
enum e_state {EXEC, CKTLD};         /* Gstate values */
enum e_ntype {GATE, PI, FB, PO};    /* column 1 of circuit format */
enum e_gtype {IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND,XNOR, BUFFER};   /* gate types */

struct cmdstruc {
   char name[MAXNAME];        /* command syntax */
   void (*fptr)();            /* function pointer of the commands */
   enum e_state state;        /* execution state sequence */
};

typedef struct n_struc {
    unsigned indx;             /* node index(from 0 to NumOfLine - 1 */
    unsigned num;              /* line number(May be different from indx */
    enum e_ntype ntype;
    enum e_gtype type;         /* gate type */
    unsigned fin;              /* number of fanins */
    unsigned fout;             /* number of fanouts */
    struct n_struc **unodes;   /* pointer to array of up nodes */
    struct n_struc **dnodes;   /* pointer to array of down nodes */
    int level;                 /* level of the gate output */
	int value;				   /*value for each node*/
} NSTRUC;                     
/*===========Functions for sim================*/
void readfile();
void extractfilename();
void circuit_value_calculation();
void outfilewriting();
string inputFilename;
string outputFilename;

/*----------------- Command definitions ----------------------------------*/
#define NUMFUNCS 6
void cread(), pc(), help(), quit(),lev(),logicsim();
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   {"LEV", lev, CKTLD},
   {"LOGICSIM",logicsim,CKTLD}
};

/*------------------------------------------------------------------------*/
enum e_state Gstate = EXEC;     /* global exectution sequence */
NSTRUC *Node;                   /* dynamic array of nodes */
NSTRUC **Pinput;                /* pointer to array of primary inputs */
NSTRUC **Poutput;               /* pointer to array of primary outputs */
int Nnodes;                     /* number of nodes */
int Npi;                        /* number of primary inputs */
int Npo;                        /* number of primary outputs */
int Done = 0;                   /* status bit to terminate program */
char *cp;              
char inFile[MAXLINE];

/*----------------------------------vi--------------------------------------*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: shell
description:
    This is the main program of the simulator. It displays the prompt, reads
    and parses the user command, and calls the corresponding routines.
    Commands not reconized by the parser are passed along to the shell.
    The command is executed according to some pre-determined sequence.
    For example, we have to read in the circuit description file before any
    action commands.  The code uses "Gstate" to check the execution
    sequence.
    Pointers to functions are used to make function calls which makes the
    code short and clean.
-----------------------------------------------------------------------*/

std::size_t strlen(const char* start) {
   const char* end = start;
   for( ; *end != '\0'; ++end)
      ;
   return end - start;
}


/*-----------------------------------------------------------------------
input: gate type
output: string of the gate type
called by: pc
description:
    The routine receive an integer gate type and return the gate type in
    character string.
-----------------------------------------------------------------------*/
std::string gname(int tp){
    switch(tp) {
        case 0: return("PI");
        case 1: return("BRANCH");
        case 2: return("XOR");
        case 3: return("OR");
        case 4: return("NOR");
        case 5: return("NOT");
        case 6: return("NAND");
        case 7: return("AND");
        case 8: return("XNOR");
        case 9: return("BUFFER");
    }
    return "";
}


/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
    The routine prints ot help inormation for each command.
-----------------------------------------------------------------------*/
void help(){
    printf("READ filename - ");
    printf("read in circuit file and creat all data structures\n");
    printf("PC - ");
    printf("print circuit information\n");
    printf("HELP - ");
    printf("print this help information\n");
    printf("QUIT - ");
    printf("stop and exit\n");
    printf("LOGICSIM - ");
    printf("simulate the logic circuit and output the results\n");
}


/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
    Set Done to 1 which will terminates the program.
-----------------------------------------------------------------------*/
void quit(){
    Done = 1;
}

/*======================================================================*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
    This routine clears the memory space occupied by the previous circuit
    before reading in new one. It frees up the dynamic arrays Node.unodes,
    Node.dnodes, Node.flist, Node, Pinput, Poutput, and Tap.
-----------------------------------------------------------------------*/
void clear(){
    int i;
    for(i = 0; i<Nnodes; i++) {
        free(Node[i].unodes);
        free(Node[i].dnodes);
    }
    free(Node);
    free(Pinput);
    free(Poutput);
    Gstate = EXEC;
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
    This routine allocatess the memory space required by the circuit
    description data structure. It allocates the dynamic arrays Node,
    Node.flist, Node, Pinput, Poutput, and Tap. It also set the default
    tap selection and the fanin and fanout to 0.
-----------------------------------------------------------------------*/
void allocate(){
    int i;
    Node = (NSTRUC *) malloc(Nnodes * sizeof(NSTRUC));
    Pinput = (NSTRUC **) malloc(Npi * sizeof(NSTRUC *));
    Poutput = (NSTRUC **) malloc(Npo * sizeof(NSTRUC *));
    for(i = 0; i<Nnodes; i++) {
        Node[i].indx = i;
        Node[i].fin = Node[i].fout = 0;
    }
}


/*-----------------------------------------------------------------------
input: circuit description file name
output: nothing
called by: main
description:
    This routine reads in the circuit description file and set up all the
    required data structure. It first checks if the file exists, then it
    sets up a mapping table, determines the number of nodes, PI's and PO's,
    allocates dynamic data arrays, and fills in the structural information
    of the circuit. In the ISCAS circuit description format, only upstream
    nodes are specified. Downstream nodes are implied. However, to facilitate
    forward implication, they are also built up in the data structure.
    To have the maximal flexibility, three passes through the circuit file
    are required: the first pass to determine the size of the mapping table
    , the second to fill in the mapping table, and the third to actually
    set up the circuit information. These procedures may be simplified in
    the future.
-----------------------------------------------------------------------*/
std::string inp_name = "";
void cread(){
    char buf[MAXLINE];
    int ntbl, *tbl, i, j, k, nd, tp, fo, fi, ni = 0, no = 0;
    FILE *fd;
    NSTRUC *np;
    cp[strlen(cp)-1] = '\0';
    if((fd = fopen(cp,"r")) == NULL){
        printf("File does not exist!\n");
        return;
    }
    inp_name = cp;
    inputFilename=cp;
    if(Gstate >= CKTLD) clear();
    Nnodes = Npi = Npo = ntbl = 0;
    while(fgets(buf, MAXLINE, fd) != NULL) {
        if(sscanf(buf,"%d %d", &tp, &nd) == 2) {
            if(ntbl < nd) ntbl = nd;
            Nnodes ++;
            if(tp == PI) Npi++;
            else if(tp == PO) Npo++;
        }
    }
    tbl = (int *) malloc(++ntbl * sizeof(int));
    
    fseek(fd, 0L, 0);
    i = 0;
    while(fgets(buf, MAXLINE, fd) != NULL) {
        if(sscanf(buf,"%d %d", &tp, &nd) == 2) tbl[nd] = i++;
    }
    allocate();

    fseek(fd, 0L, 0);
    while(fscanf(fd, "%d %d", &tp, &nd) != EOF) {
        np = &Node[tbl[nd]];
        np->num = nd;
        
        if(tp == PI) Pinput[ni++] = np;
        else if(tp == PO) Poutput[no++] = np;
        
        switch(tp) {
            case PI:
            case PO:
            case GATE:
                fscanf(fd, "%d %d %d", &np->type, &np->fout, &np->fin);
                break;
            case FB:
                np->fout = np->fin = 1;
                fscanf(fd, "%d", &np->type);
                break;
            default:
                printf("Unknown node type!\n");
                exit(-1);
        }
        np->unodes = (NSTRUC **) malloc(np->fin * sizeof(NSTRUC *));
        np->dnodes = (NSTRUC **) malloc(np->fout * sizeof(NSTRUC *));
        for(i = 0; i < np->fin; i++) {
            fscanf(fd, "%d", &nd);
            np->unodes[i] = &Node[tbl[nd]];
        }
        for(i = 0; i < np->fout; np->dnodes[i++] = NULL);
    }
    for(i = 0; i < Nnodes; i++) {
        for(j = 0; j < Node[i].fin; j++) {
            np = Node[i].unodes[j];
            k = 0;
            while(np->dnodes[k] != NULL) k++;
            np->dnodes[k] = &Node[i];
        }
    }
    fclose(fd);
    Gstate = CKTLD;
    printf("==> OK");
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main
description:
    The routine prints out the circuit description from previous READ command.
-----------------------------------------------------------------------*/
void pc(){
    int i, j;
    NSTRUC *np;
    std::string gname(int);
   
    printf(" Node   Type \tIn     \t\t\tOut    \n");
    printf("------ ------\t-------\t\t\t-------\n");
    for(i = 0; i<Nnodes; i++) {
        np = &Node[i];
        printf("\t\t\t\t\t");
        for(j = 0; j<np->fout; j++) printf("%d ",np->dnodes[j]->num);
        printf("\r%5d  %s\t", np->num, gname(np->type).c_str());
        for(j = 0; j<np->fin; j++) printf("%d ",np->unodes[j]->num);
        printf("\n");
    }
    printf("Primary inputs:  ");
    for(i = 0; i<Npi; i++) printf("%d ",Pinput[i]->num);
    printf("\n");
    printf("Primary outputs: ");
    for(i = 0; i<Npo; i++) printf("%d ",Poutput[i]->num);
    printf("\n\n");
    printf("Number of nodes = %d\n", Nnodes);
    printf("Number of primary inputs = %d\n", Npi);
    printf("Number of primary outputs = %d\n", Npo);
}
/*=====================================Levelizer====================================*/
void lev() {
   for(int i=0;i<Nnodes;i++){
        Node[i].level=-1; 
   }
   for(int i=0;i<Npi;i++){
        Pinput[i]->level=0;
   }
    int node_flag=1;//1 means we check all ndoes for one iteration.
    while(node_flag){
        node_flag=0;// we are gonna check, let flag 0

        for(int i=0;i<Nnodes;i++){
            if(Node[i].level==-1){
                int all_up_nodes=1;// the flag means that we assume up nodes finish its level are gonna 
                int level=-1;//the value after checking up nodes and update level
                for(int j=0;j<Node[i].fin;j++){//check up nodes
                    if(Node[i].unodes[j]->level==-1){// it means not all up nodes are evaluated 
                        all_up_nodes=0;
                        break;
                    }
                    level=max(level,Node[i].unodes[j]->level);
                }    
                if(all_up_nodes){ //we finish checking all up nodes  
                    Node[i].level=level+1;
                    node_flag=1;
                }
            }
        }
    }
    /*------------------------Naming---------------------------------*/
    // Name Circuit in the file
    string cir_name;
    stringstream ss(cp);
    getline(ss,cir_name,'_');
    ofstream out((cir_name+"_lev.txt").c_str());
    if(!out.is_open()){
        cerr<<"File does not exist!\n";
        return;
    }
    out<<cir_name<<endl;
    out<<"#PI: "<<Npi<<endl;
    out<<"#PO: "<<Npo<<endl;
    out<<"#Nodes: "<<Nnodes<<endl;
    int num_gates=0;
     for (int i = 0; i < Nnodes; i++) {
        if (Node[i].type == XOR || Node[i].type == OR || Node[i].type == NOR ||
            Node[i].type == NOT || Node[i].type == NAND || Node[i].type == AND) {
            num_gates++;
        }
    }
   out<<"#Gates: "<<num_gates<<endl;

    for (int i = 0; i < Nnodes; i++) {
        out<<Node[i].num<<" "<<Node[i].level<<endl;
    }

    out.close();
     for (int i = 0; i < Nnodes; i++) {
        cout << "Node " << Node[i].num << " is at level " << Node[i].level << endl;
    }

 
}

/*===========================Logic Simulator*===========================*/
//Those void functions below are used in logicsim()
bool sort_input(const NSTRUC* a, NSTRUC* b){
    return a->num < b->num;
}
//Read the input File
void readfile(){
    cout<<endl<<"******Start read the input file**********"<<endl;
    ifstream circuitfile(inputFilename.c_str());
    if(!circuitfile.is_open()){
        cerr<<"ERROR! We cannot read the input file!!"<<endl;
        return;
    }
    cout << "Name of the input file: " << inputFilename << endl;
    //Find the max Node ID in the circuit
    int max_PI_ID=0;
    string line;
    while(getline(circuitfile, line)){
        stringstream se(line);
        string data;
        getline(se, data, ',');
        int PI_ID;
        stringstream(data) >> PI_ID;
        if (PI_ID > max_PI_ID) {
            max_PI_ID = PI_ID;
        }
    }
    while (getline(circuitfile, line)) {
        cout <<"The content in input file:"<< line << endl;
    }//for test

    circuitfile.clear(); //clear array 
    circuitfile.seekg(0);// read the input file in pos 0
    
    vector<int> input_content(max_PI_ID+1, -1); 
    int PI_ID, PI_value,idx=0;
    while(getline(circuitfile, line)){//Loading the value in inputfile xxx.txt
        if(idx<Npi){
            stringstream ss(line);
            string data;
            getline(ss,data,',');
            stringstream(data)>>PI_ID;//Change string to the integer
            getline(ss,data,',');
            stringstream(data)>>PI_value;
            input_content[PI_ID]=PI_value;
            cout<<"PI_ID "<<PI_ID<<" PI_value "<<input_content[PI_ID]<<endl;
            idx++;
        }
        else{
            cerr << "Error: More primary inputs than expected!" << endl;
            break;
        }
    }
    circuitfile.close();
    //Sort Input based on PI_ID
    std::sort(Pinput, Pinput+Npi, sort_input);
    //Arrange the order of PI Nodes
    for(int i=0;i<Npi;i++){
        int npi_num=Pinput[i]->num;
        if(npi_num <= max_PI_ID && input_content[npi_num] != -1){
            Pinput[i]->value=input_content[npi_num];
        }
    }
    //Check the values we load on terminal
    for(int j=0;j<Npi;j++){
        cout<<"PI No."<<j<<", "<<Pinput[j]->value<<endl;
    }
    cout<<"************Finish Read values to PI in readfile()***********"<<endl<<endl;
    
}

bool gatefunction(enum e_gtype type, vector<bool>inputvalue){//IPT, BRCH XOR, OR, NOR, NOT, NAND, AND,XNOR, BUFFER
    bool result;
    switch(type){
        cout<<"*******New section to analyze gates in gatefunction()******"<<endl;
        case BRCH:
            result=inputvalue[0];
            break;
        case OR:
            result=0;//noncontrolling value
            for(size_t i=0;i<inputvalue.size();i++){
                result|=inputvalue[i];
            }
            break;
        case XOR:
            result=0;
            for(size_t i=0;i<inputvalue.size();i++){
                result^=inputvalue[i];
            }   
            break; 
        case NOR:
            result=0;
            for(size_t i=0;i<inputvalue.size();i++){
                result|=inputvalue[i];
            }   
            result=!result;
            break; 
         case NOT:
            result=!inputvalue[0]; 
            break;
        case NAND:
            result=1;
            for(size_t i=0;i<inputvalue.size();i++){
                result&=inputvalue[i];
            }
            result=!result;    
            break;
        case AND:
            result=1;
            for(size_t i=0;i<inputvalue.size();i++){
                result&=inputvalue[i];
            }    
            break;
        case XNOR:
            result=0;
            for(size_t i=0;i<inputvalue.size();i++){
                result^=inputvalue[i];
            }    
            result=!result; 
            break;
        case BUFFER:
            result = inputvalue[0];
            break;
        
        default:
        cerr<<"Error in Logic Gate Calculation"<<endl<<endl;;
        break;
    }
    return result;
}
void circuit_value_calculation(){
    cout<<"*****Start gate calculation in circuit_value_calculation()*****"<<endl;
    //Find the max level in the circuit because we want level-by-level driven simulator
    int max_level=0;
    for(int i=0;i<Nnodes;i++){
        if(Node[i].level>max_level){
            max_level=Node[i].level;
        }
    }
    for(int currentLevel=0;currentLevel<=max_level;currentLevel++){
        cout << "Processing gates at level: " << currentLevel<<endl;
        for(int i=0;i<Nnodes;i++){
            if(Node[i].level!=currentLevel)continue;
            if (Node[i].type == IPT) {
                cout << "Skipping primary input node: " << Node[i].num << endl;
                continue;
            }
            vector<bool>inputvalue;
            if(Node[i].type==BRCH||Node[i].type==XOR||Node[i].type==OR||Node[i].type==NOR
            ||Node[i].type==NOT||Node[i].type==NAND||Node[i].type==AND||Node[i].type==XNOR||
            Node[i].type==BUFFER){
                inputvalue.clear(); 
                for(int j=0;j<Node[i].fin;j++){
                    if(Node[i].unodes[j]!=NULL ){
                        inputvalue.push_back(Node[i].unodes[j]->value);//store all unodes value to value array
                    }
                    else {
                         cerr << "Error: unodes[" << j << "] for Node[" << i << "] is NULL!" << endl;
                    }
                }
                if(!inputvalue.empty())  {  
                    bool result=gatefunction(Node[i].type, inputvalue); 
                    Node[i].value=result; 
                    cout<< "This "<<gname(Node[i].type)<<" output is "<<Node[i].value<<endl;  
                }
            }
        }
    }
    cout<<"*****Finish gate calculation in circuit_value_calculation() and gatefunction()*****"<<endl<<endl;
}
void outfilewriting(){
    cout<<"*****Start Writing the output file****"<<endl;
    ofstream outfile(outputFilename.c_str());
    cout<<"Output File Name is "<<outputFilename<<endl;
    if(!outfile){
        cerr<<"Cannot open a output file to write"<<endl;
        return;
    }

    for(int i=0;i<Npo;i++){
        outfile<< Poutput[i]->num<<","<<Poutput[i]->value<<endl;
        cout<< "The gate of Primary Output is "<<gname(Poutput[i]->type)<<",Node number is "<<Poutput[i]->num<<", the gateoutput is "<<Poutput[i]->value<<endl;
    }
    outfile.close();
    cout<<"*****Finish Writing the output file*****"<<endl<<endl;
}
//Final function we want: logicsim()
void logicsim(){
    stringstream st(cp);
    string inputfile, outputfile;
    st>>inputfile;
    st>>outputfile;
    inputFilename=inputfile;
    outputFilename=outputfile;
    readfile();
    circuit_value_calculation();
    outfilewriting();
}
/*========================= End of program ============================*/
