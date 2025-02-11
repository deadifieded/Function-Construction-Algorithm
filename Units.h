struct Unit {
    int input_size;
    int output_size;
    bool bi_symmetric;
    const char* latex_stuff;

    virtual void func(unsigned char**, unsigned char** output) = 0;
    virtual void func_n(unsigned char** input, unsigned char* const* const_output, int n)
    {
        unsigned char** output = new unsigned char* [output_size];

        for (int i = 0; i < output_size;i++)
        {
            output[i] = const_output[i];
        }

        for (int i = 0; i < n; i++)
        {
            func(input, output);
            for (int i = 0; i < input_size; i++)
            {
                input[i]++;
            }
            for (int i = 0; i < output_size; i++)
            {
                output[i]++;
            }
        }
    }
    virtual void fasm(asmjit::x86::Compiler*, asmjit::x86::Gp**, asmjit::x86::Gp*) = 0;
};

/*
struct Blank : Unit {
    static const int input_size = ;
    static const int output_size = ;

    static void func(unsigned char** input, unsigned char* output)
    {

    }

    static void fasm(asmjit::x86::Compiler* compile, asmjit::x86::Gp** input, asmjit::x86::Gp* output, asmjit::x86::Gp* write_ptr)
    {

    }
};
*/

struct InputUnit : Unit
{
    InputUnit()
    {
        input_size = 0;
        output_size = 1;
        bi_symmetric = false;
        latex_stuff = "I";
    }

    void func(unsigned char** input, unsigned char** output) override
    {
        throw("tries to run Input Unit");
    }

    void fasm(asmjit::x86::Compiler* compiler, asmjit::x86::Gp** input, asmjit::x86::Gp* output) override
    {
        throw("tries to fasm Input Unit");
    }
};

struct Add_8 : Unit {
    Add_8()
    {
        input_size = 2;
        output_size = 1;
        bi_symmetric = true;
        latex_stuff = "+";
    }

    void func(unsigned char** input, unsigned char** output) override
    {
        //std::cout << (int)*input[0] << " + " << (int)*input[1] << " = " << *input[0] + *input[1] << "\n";
        *output[0] = *input[0] + *input[1];
    }

    void fasm(asmjit::x86::Compiler* compiler, asmjit::x86::Gp** input, asmjit::x86::Gp* output) override
    {
        compiler->mov(output[0], *input[0]);
        compiler->add(output[0], *input[1]);
        //std::cout << write << "\n";
    }
};

struct Add_16 : Unit {

    Add_16()
    {
        input_size = 2;
        output_size = 2;
        bi_symmetric = true;
    }

    static void func(unsigned char** input, unsigned char* output)
    {
        *reinterpret_cast<unsigned short*>(output) = *input[0] + *input[1];
    }

    static void fasm(asmjit::x86::Compiler* compiler, asmjit::x86::Gp** input, asmjit::x86::Gp* output)
    {
         
    }
};
