#include "aca2009.h"
#include <systemc.h>
#include <iostream>
using namespace std;
static const uint MEM_SIZE = 0x1ff;//0xffffffff;  // 4GB
static const uint BLOCK_SIZE = 32 ; //bytes
static const uint WAY_SIZE = 2;
static const uint CACHE_SIZE = 256;//32768;  // 32KB = 0x8000
static const uint SET_SIZE = CACHE_SIZE/BLOCK_SIZE;
static const uint SET_PER_WAY = SET_SIZE/WAY_SIZE;

sc_trace_file *wf = sc_create_vcd_trace_file("wf_cache");

SC_MODULE(CACHE){
public:
  enum function{
    func_read,
    func_write
  };
  enum retcode{
    ret_read_done,
    ret_write_done
  };
  enum status{
    status_miss,
    status_hit
  };
  struct cache_block{
    int data;    //[BLOCK_SIZE];
    uint tag;
    bool valid;
    sc_core::sc_time time_stamp;
  };
  sc_uint<WAY_SIZE> way[SET_PER_WAY];
  sc_in<bool>     port_clk;
  sc_in<function> port_func;
  sc_in<uint>     port_addr;
  sc_out<retcode> port_done;
  sc_inout_rv<32> port_data;

  // local variables for cache
  uint set_number;
  uint addr_tag;
  uint hit_line;
  uint hit_way;
  uint lru_way;
  uint miss_line_replace;
  status cache_status;
  function f;

  SC_CTOR(CACHE){
    SC_THREAD(execute);
    sensitive<<port_clk.pos();
    dont_initialize();
    //m_data = new int[MEM_SIZE];
    //cache_val =  new struct cache_block[WAY_SIZE][SET_PER_WAY];
  }
  ~CACHE(){
    //delete[] cache_val;
  }
private:
  //int* m_data;
  struct cache_block cache_val[WAY_SIZE][SET_PER_WAY];
  void execute(){
    while(true){
      wait(port_func.value_changed_event());
      f = port_func.read();
      uint addr = port_addr.read();
      int data = 0;
  //  cout<< sc_time_stamp() << " @ cache func  " << f << "  addr " << addr << "  ret " << port_done.read() << "  status  " << cache_status<<endl;
      if(f == func_write){
        data = port_data.read().to_int();
      }
      //Look for presence in memory
      /*set_number = (addr>>5)&0x000003ff;
      addr_tag = addr >> 10;*/
      set_number = (addr>>5)&0x000003;   // CHANGE ADDRESSES
      addr_tag = addr >> 7;
      cache_status = status_miss;
      hit_way = 0xFF;
      for (size_t i = 0; i < WAY_SIZE; i++) {
        if(addr_tag == cache_val[i][set_number].tag){
          cache_status = status_hit;
          hit_way = i;
          break;
        }
      }
      //simulate cache read/write delay
      if(cache_status == status_hit)
        wait(10000);         // CHANGE SIMULATION TIMES
      else
        wait(10000);

      if (f == func_read) {
        if(cache_status == status_hit){
          port_data.write(cache_val[hit_way][set_number].data);
          cache_val[hit_way][set_number].time_stamp = sc_time_stamp();
        }
        else{
          lru_way = 0;
          for (size_t i = 1; i < WAY_SIZE; i++) {
              if(cache_val[i][set_number].time_stamp < cache_val[i-1][set_number].time_stamp){
                lru_way = i;
              }
          }
          cache_val[lru_way][set_number].data = 0xffffffff;
          cache_val[lru_way][set_number].tag = addr_tag;
          port_data.write(cache_val[lru_way][set_number].data);
          cache_val[lru_way][set_number].time_stamp = sc_time_stamp();
        }
        port_done.write(ret_read_done);
        wait();
        port_data.write("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
      }
      else{
        if(cache_status == status_hit){
          cache_val[hit_way][set_number].data = data;
          cache_val[hit_way][set_number].time_stamp = sc_time_stamp();
        }
        else{
          lru_way = 0;
          for (size_t i = 1; i < WAY_SIZE; i++) {
              if(cache_val[i][set_number].time_stamp < cache_val[i-1][set_number].time_stamp){
                lru_way = i;
              }
          }
          cache_val[lru_way][set_number].tag = addr_tag;
          cache_val[lru_way][set_number].data = data;
          cache_val[lru_way][set_number].time_stamp = sc_time_stamp();
        }
        port_done.write(ret_write_done);
      }
      cout<< "tag  " << addr_tag << "  set  "<< set_number <<"  hit way " << hit_way <<"  LRU_WAY " << lru_way << endl;
      cout<< sc_time_stamp() << " æ cache func  " << f << "  addr " << hex << addr << "  ret " << port_done.read()<< "  status  " << cache_status<<endl;
    }
  }
};


SC_MODULE(CPU){
public:
  sc_in<bool>               port_clk;
  sc_in<CACHE::retcode>     port_memdone;
  sc_out<CACHE::function>   port_memfunc;
  sc_out<uint>              port_memaddr;
  sc_inout_rv<32>           port_memdata;
  SC_CTOR(CPU){
    SC_THREAD(execute);
    sensitive << port_clk.pos();
    dont_initialize();
  }
private:
  void execute() {
    while(true){
      wait();
      CACHE::function f = (rand()%10)<5 ? CACHE::func_read :
                            CACHE::func_write;
      int addr = (rand() % MEM_SIZE);
      port_memaddr.write(addr);
      port_memfunc.write(f);
      if (f == CACHE::func_write) {
        port_memdata.write(rand());
        wait();
        port_memdata.write("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
      }
  //    cout<< sc_time_stamp() << " @  CPU func  " << f << "  addr " << addr << "  ret " << port_memdone.read()<<endl;
      wait(port_memdone.value_changed_event());
  //    cout<< sc_time_stamp() << " æ CPU func  " << f << "  addr " << addr << "  ret " << port_memdone.read()<<endl;
      //Advance a cycle in simulation time
      wait();
    }
  }
};


int sc_main(int argc, char* argv[]){
  try{
    CACHE cache("cache");
    CPU cpu("cpu");

    //signals
    sc_buffer<CACHE::function>   sigmemfunc;
    sc_buffer<CACHE::retcode>    sigmemdone;
    sc_signal<uint>              sigmemaddr;
    sc_signal_rv<32>             sigmemdata;
    sc_clock clk;
    cache.port_func(sigmemfunc);
    cache.port_addr(sigmemaddr);
    cache.port_data(sigmemdata);
    cache.port_done(sigmemdone);

    cpu.port_memfunc(sigmemfunc);
    cpu.port_memaddr(sigmemaddr);
    cpu.port_memdata(sigmemdata);
    cpu.port_memdone(sigmemdone);

    cache.port_clk(clk);
    cpu.port_clk(clk);
    sc_trace(wf, cache.set_number, "set_number");
    sc_trace(wf, cache.cache_status, "Hit/Miss");
    sc_trace(wf, cache.hit_way, "hit_way");
    sc_trace(wf, cache.lru_way, "lru_way");
    sc_trace(wf, cache.f, "Rd/Wr");
    sc_trace(wf, cache.port_addr, "Address");
    //sc_trace(wf, clk, "clock");

    cout<<"running mem simulation, ctrl+c to exit"<<endl;
    //Start simulation
    sc_start();
    }
    catch (exception& e){
        cerr << e.what() << endl;
    }
  return 0;
}
