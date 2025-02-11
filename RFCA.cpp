#define ASM_JIT_EMBED
#define ASMJIT_NO_FOREIGN

#include <iostream>
#include "asmjit_src/asmjit/asmjit.h"
#include <chrono>
#include <random>
#include "Units.h"

constexpr int MAX_INPUTS = 2;

unsigned long int _HID = 0;
unsigned long int _HID_INC = 1580031231;

InputUnit INPUT_UNIT = InputUnit();

int getRandGeom(int cap)
{
    int num = 0;
    int i = std::rand()+1;
    while (i % 256 == 0)
    {
        num += 8;
        i = std::rand();
    }

    while (i % 2 == 0)
    {
        num++;
        i /= 2;
    }

    if (num < cap)
        return num;
    else
        return cap-1;
}

unsigned long getHID()
{
    _HID += _HID_INC;
    return _HID;
}

void retractHID(unsigned int nof)
{
    _HID -= _HID_INC*nof;
}

struct Individual;

struct IndividualList
{
    IndividualList* next;
    IndividualList* prev;
    Individual* individual;

    IndividualList() {}

    void insert(IndividualList* prev, IndividualList* next)
    {
        this->prev = prev;
        this->next = next;
        prev->next = this;
        next->prev = this;
    }

    void take(IndividualList* individual)
    {
        individual->delink();
        individual->insert(prev, this);
    }

    void initialise()
    {
        next = this;
        prev = this;
    }

    void add(Individual* individual)
    {
        IndividualList* poop = new IndividualList();
        poop->individual = individual;
        poop->insert(prev, this);
    }

    IndividualList* delink()
    {
        prev->next = next;
        next->prev = prev;
        return prev;
    }
};

struct Individual 
{
    Individual** inputs;
    int* input_indexes;
    IndividualList outputs;
    Unit* unit;
    unsigned char** data;
    unsigned long int HID;
    int depth;
    bool toggle;

    Individual(Unit* unit, Individual** inputs, int* input_indexes)
    {
        outputs = IndividualList();
        this->unit = unit;
        this->inputs = inputs;
        this->input_indexes = input_indexes;
        HID = getHID();
        toggle = false;
    }

    ~Individual()
    {
        delete[] inputs;
    }

    void addOutput(Individual* individual)
    {
        outputs.add(individual);
    }
};

struct IndividualTree
{
    IndividualTree* less_than;
    IndividualTree* greater_than;
    Individual* individual;

    IndividualTree()
    {
        less_than = nullptr;
        greater_than = nullptr;
        individual = nullptr;
    }

    bool insert(Individual* new_individual)
    {
        IndividualTree* branch = this;

        Unit* unit = new_individual->unit;
        unsigned long int HIDs[MAX_INPUTS] = {};
        int unit_input_size = unit->input_size;
        
        for (int i = 0; i < unit->input_size; i++)
        {
            //std::cout << new_individual->inputs[i] << "\n";
            HIDs[i] = new_individual->inputs[i]->HID;
        }

        Individual* comp_individual = branch->individual;

        while (comp_individual != nullptr)
        {
            Unit* comp_unit = comp_individual->unit;
            if (unit < comp_unit)
            {
                branch = branch->less_than;
                comp_individual = branch->individual;
                continue;
            }
            if (unit > comp_unit)
            {
                branch = branch->greater_than;
                comp_individual = branch->individual;
                continue;
            }

            int i = 0;

            while (true)
            {
                unsigned long int comp_HID = comp_individual->inputs[i]->HID;
                unsigned long int HID = HIDs[i];

                if (HID < comp_HID)
                {
                    branch = branch->less_than;
                    comp_individual = branch->individual;
                    break;
                }
                if (HID > comp_HID)
                {
                    branch = branch->greater_than;
                    comp_individual = branch->individual;
                    break;
                }

                i++;

                if (i >= unit_input_size)
                {
                    return false;
                }
            }

            
        }

        branch->individual = new_individual;
        branch->less_than = new IndividualTree();
        branch->greater_than = new IndividualTree();
        return true;
    }
};

class MyErrorHandler : public asmjit::ErrorHandler {
public:
    void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
        printf("AsmJit error: %s\n", message);
    }
};

MyErrorHandler myErrorHandler;

struct Layer {
    int input_size;
    int output_size;
    int unit_output_size;
    int max_input_size;

    int nof_units;
    Unit** units;
    int* connections;

    Layer(int input_size, int output_size, int nof_units, Unit** units, int* connections) : input_size(input_size), output_size(output_size), nof_units(nof_units), units(units), connections(connections)
    {
        unit_output_size = 0;
        max_input_size = 0;

        int temp;
        Unit* unit;
        for (int i = 0; i < nof_units; i += 1)
        {
            unit = units[i];
            temp = unit->input_size;
            unit_output_size += unit->output_size;
            if (temp > max_input_size)
            {
                max_input_size = temp;
            }
        }
    }

    unsigned char* getMemory(int nof_blocks = 1)
    {
        return new unsigned char[input_size + unit_output_size + output_size + max_input_size * sizeof(unsigned char*)];
    }

    int getSize()
    {
        return input_size + unit_output_size + output_size + max_input_size * sizeof(unsigned char*);
    }

    /*unsigned char* run(unsigned char** read_ptrs, unsigned char** write_ptrs)
    {
        unsigned char** connections_ptr = reinterpret_cast<unsigned char**>(write_ptr + unit_output_size + output_size);
        int connections_index = 0;

        Unit* unit;

        for (int i = 0; i < nof_units; i++)
        {
            unit = units[i];

            for (int j = 0; j < unit->input_size; j += 1)
            {
                connections_ptr[j] = read_ptr + connections[connections_index];
                //std::cout << (int)connections_ptr[j] << "\n";
                connections_index++;
            }

            //std::cout << "\n" << (int)write_ptr << "\n\n";
            unit->func(connections_ptr, write_ptr);
            write_ptr += unit->output_size;
        }

        for (int i = 0; i < output_size; i++)
        {
            *write_ptr = *(read_ptr + connections[connections_index]);
            //std::cout << (int)(read_ptr + connections[connections_index])<<"\n";
            write_ptr+=1;
            connections_index+=1;
        }
        //std::cout << "\n";

        return write_ptr - output_size;
    }*/

    void (*getEfficientFunction(asmjit::JitRuntime *jr))(unsigned char**, unsigned char**)
    {
        asmjit::CodeHolder code;
        code.init(jr->environment(), jr->cpuFeatures());
        code.setErrorHandler(&myErrorHandler);
        asmjit::x86::Compiler compiler(&code);

        asmjit::FuncNode* func_node = compiler.addFunc(asmjit::FuncSignature::build<
            void,
            uint8_t**,
            uint8_t**>());

        asmjit::x86::Gp* gps = new asmjit::x86::Gp[input_size + unit_output_size];
        //std::cout << input_size + unit_output_size + output_size << "\n";
        asmjit::x86::Gp* gps_output_ptr = gps;

        asmjit::x86::Gp read_ptr_ptr = compiler.newUIntPtr();
        asmjit::x86::Gp read_ptr = compiler.newUIntPtr();

        asmjit::x86::Gp write_ptr_ptr = compiler.newUIntPtr();
        asmjit::x86::Gp write_ptr = compiler.newUIntPtr();

        func_node->setArg(0, read_ptr_ptr);
        func_node->setArg(1, write_ptr_ptr);
        for (int i = 0; i < input_size; i++)
        {
            //std::cout << i << "\n";
            *gps_output_ptr = compiler.newUInt8();
            compiler.mov(read_ptr, asmjit::x86::qword_ptr(read_ptr_ptr));
            compiler.mov(*gps_output_ptr, asmjit::x86::byte_ptr(read_ptr));
            compiler.add(read_ptr_ptr, 8);
            gps_output_ptr++;
        }

        asmjit::x86::Gp** connections_ptr = new asmjit::x86::Gp*[max_input_size];
        int connections_index = 0;

        for (int i = 0; i < nof_units; i++)
        {
            Unit* unit = units[i];

            for (int j = 0; j < unit->output_size; j++)
            {
                gps[input_size + i + j] = compiler.newUInt8();
            }
            

            for (int j = 0; j < unit->input_size; j += 1)
            {
                //connections_ptr[j] = read_ptr + connections[connections_index];
                connections_ptr[j] = &gps[connections[connections_index]];
                connections_index++;
            }

            unit->fasm(&compiler, connections_ptr, gps_output_ptr);
            gps_output_ptr += unit->output_size;
        }

        for (int i = 0; i < output_size; i++)
        {
            //*write_ptr = *(read_ptr + connections[connections_index]);
            compiler.mov(write_ptr, asmjit::x86::qword_ptr(write_ptr_ptr));
            compiler.mov(asmjit::x86::byte_ptr(write_ptr), gps[connections[connections_index]]);
            if (i < output_size-1)
                compiler.add(write_ptr_ptr, 8);
            connections_index += 1;
        }

        compiler.ret();
        
        compiler.endFunc();
        compiler.finalize();

        //unsigned char* output = (*efficient_func)(memory);//adderTest.run(memory);

        void (*efficient_func)(unsigned char**, unsigned char**);

        auto err = jr->add(&efficient_func, &code);

        //std::cout << err << "\n";

        return efficient_func;
    }

    void (*getEfficientFunctionN(asmjit::JitRuntime* jr))(unsigned char**, unsigned char**, unsigned int) 
    {
        asmjit::CodeHolder code;
        code.init(jr->environment(), jr->cpuFeatures());
        code.setErrorHandler(&myErrorHandler);
        asmjit::x86::Compiler compiler(&code);

        asmjit::FuncNode* func_node = compiler.addFunc(asmjit::FuncSignature::build<
            void,
            uint8_t**,
            uint8_t**,
            uint32_t>());

        asmjit::x86::Gp* gps = new asmjit::x86::Gp[input_size + unit_output_size];
        //std::cout << input_size + unit_output_size + output_size << "\n";
        asmjit::x86::Gp* gps_output_ptr = gps;

        asmjit::x86::Gp read_ptr_ptr = compiler.newUIntPtr();

        asmjit::x86::Gp write_ptr_ptr = compiler.newUIntPtr();
        
        asmjit::x86::Gp count = compiler.newUInt32();

        func_node->setArg(0, read_ptr_ptr);
        func_node->setArg(1, write_ptr_ptr);
        func_node->setArg(2, count);

        asmjit::Label loop_begin = compiler.newLabel();
        asmjit::Label loop_end = compiler.newLabel();

        compiler.test(count, count);
        compiler.jz(loop_end);

        asmjit::x86::Gp* read_ptr = new asmjit::x86::Gp[input_size];

        for (int i = 0; i < input_size; i++)
        {
            read_ptr[i] = compiler.newUIntPtr();

            if (i > 0)
                compiler.add(read_ptr_ptr, 8);
            compiler.mov(read_ptr[i], asmjit::x86::qword_ptr(read_ptr_ptr));
        }

        asmjit::x86::Gp* write_ptr = new asmjit::x86::Gp[output_size];

        for (int i = 0; i < output_size; i++)
        {
            write_ptr[i] = compiler.newUIntPtr();

            if (i > 0)
                compiler.add(write_ptr_ptr, 8);
            compiler.mov(write_ptr[i], asmjit::x86::qword_ptr(write_ptr_ptr));
        }

        compiler.bind(loop_begin);
        
        for (int i = 0; i < input_size; i++)
        {
            //std::cout << i << "\n";
            *gps_output_ptr = compiler.newUInt8();
                
            compiler.mov(*gps_output_ptr, asmjit::x86::byte_ptr(read_ptr[i]));
            compiler.inc(read_ptr[i]);
            
            gps_output_ptr++;
        }

        asmjit::x86::Gp** connections_ptr = new asmjit::x86::Gp * [max_input_size];
        int connections_index = 0;

        for (int i = 0; i < nof_units; i++)
        {
            Unit* unit = units[i];

            for (int j = 0; j < unit->output_size; j++)
            {
                gps[input_size + i + j] = compiler.newUInt8();
            }


            for (int j = 0; j < unit->input_size; j += 1)
            {
                //connections_ptr[j] = read_ptr + connections[connections_index];
                connections_ptr[j] = &gps[connections[connections_index]];
                connections_index++;
            }

            unit->fasm(&compiler, connections_ptr, gps_output_ptr);
            gps_output_ptr += unit->output_size;
        }

        for (int i = 0; i < output_size; i++)
        {
            //*write_ptr = *(read_ptr + connections[connections_index]);
            compiler.mov(asmjit::x86::byte_ptr(write_ptr[i]), gps[connections[connections_index]]);
            compiler.inc(write_ptr[i]);
            connections_index += 1;
        }

        compiler.dec(count);
        compiler.jnz(loop_begin);

        compiler.bind(loop_end);

        compiler.ret();

        compiler.endFunc();
        compiler.finalize();

        //unsigned char* output = (*efficient_func)(memory);//adderTest.run(memory);

        void (*efficient_func)(unsigned char**, unsigned char**, unsigned int);

        auto err = jr->add(&efficient_func, &code);

        //std::cout << err << "\n";

        return efficient_func;
    }
};

struct FCA_V1
{
    IndividualTree individual_tree;
    int nof_outputs;
    int nof_inputs;
    int nof_samples;
    unsigned char** input_values;
    unsigned char** output_values;
    int max_search_depth;
    int breed_depth_cap;
    int nof_units;
    Unit** unit_set;
    bool finished;
    int total_checked;
    
    IndividualList* list_individuals_at_depth;
    IndividualTree tree_individuals;
    int search_offset;
    IndividualList*** input_ptrs;
    int max_input_size;
    IndividualList output_individuals;
    IndividualList solved_outputs;


    FCA_V1(Unit** unit_set, int nof_units, int nof_inputs, int nof_outputs, int nof_samples, unsigned char** input_values, unsigned char** output_values, int max_search_depth) 
        : unit_set(unit_set),nof_units(nof_units), nof_inputs(nof_inputs), nof_outputs(nof_outputs), nof_samples(nof_samples), input_values(input_values), output_values(output_values), max_search_depth(max_search_depth)
    {
        finished = false;
        max_input_size = 0;
        total_checked = 0;

        for (int i = 0; i < nof_units; i++)
        {
            int temp = unit_set[i]->input_size;
            if (temp > max_input_size)
                max_input_size = temp;
        }

        list_individuals_at_depth = new IndividualList[max_search_depth]{};
        solved_outputs = IndividualList();
        solved_outputs.initialise();
        output_individuals = IndividualList();
        output_individuals.initialise();
        tree_individuals = IndividualTree();

        

        for (int i = 0; i < max_search_depth; i++)
        {
            list_individuals_at_depth[i] = IndividualList();
            //std::cout << &list_individuals_at_depth[i] << "\n";
            list_individuals_at_depth[i].initialise();
        }



        for (int i = 0; i < nof_outputs; i++)
        {
            Individual* temp = new Individual(nullptr, nullptr, nullptr);
            temp->data = output_values + i;
            output_individuals.add(temp);
        }

        for (int i = 0; i < nof_inputs; i++)
        {
            Individual* temp = new Individual(&INPUT_UNIT, nullptr, nullptr);
            temp->data = input_values + i;
            temp->depth = 0;
            list_individuals_at_depth[0].add(temp);
            test(temp,1);
        }

        input_ptrs = new IndividualList **[max_input_size];

        for (int i = 0; i < max_input_size; i++)
        {
            input_ptrs[i] = new IndividualList * [max_search_depth];

            for (int j = 0; j < max_search_depth; j++)
            {
                input_ptrs[i][j] = list_individuals_at_depth+j;
            }
        }

        breed_depth_cap = 1;
    }

    bool processNTimes(int n)
    {
        if (finished)
        {
            return true;
        }
        int* input_depth = new int[max_input_size];
        unsigned char** input_to_unit = new unsigned char* [max_input_size];
        while (n > 0)
        {
            Unit* unit = unit_set[std::rand() % nof_units];
            int unit_nof_inputs = unit->input_size;
            int unit_nof_outputs = unit->output_size;

            int depth_largest = getRandGeom(breed_depth_cap);
            if (breed_depth_cap < max_search_depth-1)
            {
                if (depth_largest == breed_depth_cap-1)
                    breed_depth_cap++;
            }

            int max_depth_input_index = std::rand() % unit_nof_inputs;

            for (int i = 0; i < unit_nof_inputs; i++)
            {
                if (i == max_depth_input_index)
                {
                    input_depth[i] = depth_largest;
                }
                else
                {
                    input_depth[i] = std::rand() % (depth_largest+1);
                }
            }

            int* input_indexes = new int[unit_nof_inputs];
            Individual** input_individuals = new Individual * [unit_nof_inputs];



            for (int i = 0; i < unit_nof_inputs; i++)
            {
                auto input = input_ptrs[i][input_depth[i]]->next;

                if (std::rand() % 2)
                {
                    input = input->next;
                }

                auto list = &list_individuals_at_depth[input_depth[i]];

                if (input == list or input == list->next)
                {
                    input = input->next;
                }

                if (input == list)
                {
                    input = input->next;
                }

                input_ptrs[i][input_depth[i]] = input;
                input_individuals[i] = input->individual;
            }

            if (unit->bi_symmetric)
            {
                if (input_individuals[1]<input_individuals[0])
                {
                    auto temp = input_individuals[0];
                    input_individuals[0] = input_individuals[1];
                    input_individuals[1] = temp;
                }
            }

            for (int i = 0; i < unit_nof_inputs; i++)
            {
                auto individual = input_individuals[i];
                input_indexes[i] = std::rand() % individual->unit->output_size;
                input_to_unit[i] = individual->data[input_indexes[i]];
            }

            Individual* individual = new Individual(unit,input_individuals, input_indexes);

            if (tree_individuals.insert(individual))
            {
                unsigned char** output_from_unit = new unsigned char* [unit_nof_outputs];

                for (int i = 0; i < unit_nof_outputs; i++)
                {
                    output_from_unit[i] = new unsigned char[nof_samples];
                }

                n--;
                unit->func_n(input_to_unit, output_from_unit, nof_samples);
                individual->data = output_from_unit;

                list_individuals_at_depth[depth_largest + 1].add(individual);
                individual->depth = depth_largest + 1;

                total_checked++;
                if (test(individual, unit_nof_outputs))
                {
                    finished = true;
                    delete[] input_depth;
                    delete[] input_to_unit;
                    return true;
                }
            }
            else
            {
                retractHID(unit->output_size);
                delete individual;
            }
        }
        delete[] input_depth;
        delete[] input_to_unit;

        return false;
    }

    bool test(Individual* individual, int nof_outputs)
    {
        bool matched = false;

        for (int k = 0; k < nof_outputs; k++)
        {
            IndividualList* output = output_individuals.next;

            while (output != &output_individuals)
            {
                unsigned char* output_data = output->individual->data[0];
                unsigned char* individual_data = individual->data[k];
                int i = 0;
                while (i < nof_samples)
                {
                    if (output_data[i] != individual_data[i])
                        break;

                    i++;
                }

                output = output->next;

                if (i == nof_samples)
                {
                    auto input_temp = new Individual * [1] {individual};
                    std::cout << "match found: " << individual->HID << "\n";
                    output->prev->individual->inputs = input_temp;
                    output->prev->individual->input_indexes = new int[1] {k};
                    solved_outputs.take(output->prev);
                    matched = true;
                }
            }
        }

        if (matched)
        {
            if (output_individuals.next == &output_individuals)
            {
                finished = true;
                return true;
            }
        }

        return false;
    }

    void getTikzCode()
    {
        std::cout << "----------------------------------------------------------------\n\nUnit addresses:\n";

        for (int i = 0; i < nof_units; i++)
        {
            
            std::cout << unit_set[i] << "\n";
                
        }

        std::cout << "\n\nInput HIDs:\n";
        auto temp = &list_individuals_at_depth[0];
        auto index = temp->next;

        while (index != temp)
        {
            std::cout << index->individual->HID << "\n";
            index = index->next;
        }

        std::cout << "\n\n Tikx Code\n";

        if (!finished)
        {
            return;
        }

        

        auto essential_units = IndividualList();

        essential_units.initialise();

        IndividualList* output = solved_outputs.next;

        while (output != &solved_outputs)
        {

            auto input = output->individual->inputs[0];

            if (!input->toggle)
            {

                essential_units.add(input);
                input->toggle = true;
            }

            output = output->next;
        }

        auto e_unit = essential_units.next;

        //fill essential units

        while (e_unit != &essential_units)
        {
            if (e_unit->individual->depth != 0)
            {
                
                //std::cout << e_unit->individual->unit << " " << e_unit->individual->HID << " ";

                for (int i = 0; i < e_unit->individual->unit->input_size;i++)
                {
                    auto i_unit = e_unit->individual->inputs[i];
                    //std::cout << i_unit->HID << " ";
                    if (!i_unit->toggle)
                    {
                        essential_units.add(i_unit);
                        i_unit->toggle = true;
                    }
                }
            }
            e_unit = e_unit->next;
        }


        //print node code

        

        unsigned long int prev_top_HID;
        unsigned long int prev_HID;

        for (int depth = 0; depth < max_search_depth; depth++)
        {
            bool first = true;
            e_unit = essential_units.next;
            while (e_unit != &essential_units)
            {
                if (e_unit->individual->depth == depth)
                {
                    const char* unit_latex = e_unit->individual->unit->latex_stuff;
                    unsigned long int HID = e_unit->individual->HID;

                    if (first and depth == 0)
                    {
                        prev_top_HID = HID;
                        prev_HID = HID;
                        std::cout << "\\node[state](" << HID << ") {$" << unit_latex << "$};\n";
                        first = false;
                    }
                    else if (first)
                    {
                        std::cout << "\\node[state, right of = " << prev_top_HID << "](" << HID << ") {$" << unit_latex << "$};\n";
                        prev_HID = HID;
                        prev_top_HID = HID;
                        first = false;
                    }
                    else
                    {
                        std::cout << "\\node[state, below of = " << prev_HID << "](" << HID << ") {$" << unit_latex << "$};\n";
                        prev_HID = HID;
                    }
                }
                e_unit = e_unit->next;
            }
            std::cout << "\n";
        }

        output = solved_outputs.next;

        int count = 1;

        while (output != &solved_outputs)
        {
           unsigned long int HID = output->individual->HID;

           if (count == 1)
           {
               std::cout << "\\node[state, right of = " << prev_top_HID << "](" << HID << ") {$\\theta_" << count << "$};\n";
               prev_HID = HID;
               }
           else
           {
               std::cout << "\\node[state, below of = " << prev_HID << "](" << HID << ") {$\\theta_" << count << "$};\n";
               prev_HID = HID;
           }
           
           output = output->next;
           count++;
        }

        std::cout << "\n\n\\draw";

        // print edge code
        output = solved_outputs.next;

        while (output != &solved_outputs)
        {
            unsigned long int HID = output->individual->HID;

            std::cout << "\n(" << output->individual->inputs[0]->HID << ")edge[] node{} (" << HID << ")";

            output = output->next;
            count++;
        }
        
        e_unit = essential_units.next;

        while (e_unit != &essential_units)
        {
            if (e_unit->individual->depth != 0)
            {
                //(a)edge[] node{} (c)
                //std::cout << e_unit->individual->unit << " " << e_unit->individual->HID << " ";

                for (int i = 0; i < e_unit->individual->unit->input_size;i++)
                {
                    auto i_unit = e_unit->individual->inputs[i];
                    if (i == 0)
                        std::cout << "\n(" << e_unit->individual->inputs[i]->HID << ")edge[bend left] node{} (" << e_unit->individual->HID << ")";
                    else
                        std::cout << "\n(" << e_unit->individual->inputs[i]->HID << ")edge[bend right] node{} (" << e_unit->individual->HID << ")";
                }
            }
            e_unit = e_unit->next;
        }

        std::cout << ";\n";
    }
};

int main()
{
    Add_8 adder = Add_8();

    asmjit::JitRuntime rt;

    

    constexpr int NUM_UNITS = 3;
    constexpr int NUM_SAMPLES = 50;

    Unit* units[NUM_UNITS] {};

    for (int i = 0; i < NUM_UNITS; i += 1)
    {
        units[i] = &adder;
    }

    int connections[2*NUM_UNITS+3] = {};

    for (int i = 0; i < NUM_UNITS; i += 1)
    {
        connections[2 * i] = i;
        connections[2 * i + 1] = i + 1;
    }

    connections[NUM_UNITS * 2] = NUM_UNITS - 1;
    connections[NUM_UNITS * 2 + 1] = NUM_UNITS;
    connections[NUM_UNITS * 2 + 2] = NUM_UNITS + 1;

    Layer adderTest(2, 3, NUM_UNITS, units, connections);

    unsigned char* memory = new unsigned char[5*NUM_SAMPLES];

    unsigned char* temp_ptr = memory;

    for (int i = 0; i < NUM_SAMPLES;i++)
    {
        memory[i] = std::rand() % 256;
        memory[NUM_SAMPLES + i] = std::rand() % 256;
    }

    unsigned char* inputs[2] = { temp_ptr, temp_ptr+NUM_SAMPLES };

    unsigned char* outputs[3] = { temp_ptr+2*NUM_SAMPLES, temp_ptr+3*NUM_SAMPLES, temp_ptr+4*NUM_SAMPLES };

    auto func = adderTest.getEfficientFunctionN(&rt);

    func(inputs, outputs, NUM_SAMPLES);

    /*IndividualTree root = IndividualTree();

    Individual input1 = Individual(nullptr, nullptr, nullptr);
    Individual input2 = Individual(nullptr, nullptr, nullptr);
    Individual input3 = Individual(nullptr, nullptr, nullptr);
    Individual input4 = Individual(nullptr, nullptr, nullptr);
    Individual input5 = Individual(nullptr, nullptr, nullptr);

    Individual* inputs[5] = { &input1, &input2, &input3, &input4, &input5};

    int total = 0;*/

    Unit* unit_set[1] = {&adder};

    FCA_V1 fca = FCA_V1(unit_set, 1, 2, 3, 2, inputs, outputs, NUM_UNITS+1);
    while (!fca.processNTimes(100))
    {
        std::cout << fca.total_checked << "\n";
    }

    std::srand(156);

    std::cout << fca.total_checked << "\n\n\n";

    fca.getTikzCode();
}
