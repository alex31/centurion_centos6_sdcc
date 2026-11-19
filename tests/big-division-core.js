'use strict';
const fs=require('node:fs'), path=require('node:path'), assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');
const {CoreEmulator}=require(root+'/build/out/emulator/server/src/core/emulator');
const machine=new CoreEmulator(),cpu=global.window.cpu,bus=global.window.bpl;
global.window.__rtcGuard=true;machine.reset();while(!cpu.at_boundry)cpu.step(false);
const input=fs.readFileSync(process.argv[2]),out=Buffer.alloc(input.length/20*44);
const names=['a','b','x','y','z','s','c','p'];
for(let pos=0,dst=0;pos<input.length;pos+=20,dst+=44){
    const sa=input[pos], sb=input[pos+1];
    for(let i=0;i<8;i++){bus.writebyte(0x5000+i,0xa5);bus.writebyte(0x5020+i,0xa5);bus.writebyte(0x5040+i,0x55);}
    for(let i=0;i<sb;i++)bus.writebyte(0x5000+i,input[pos+4+8-sb+i]);
    for(let i=0;i<sa;i++)bus.writebyte(0x5020+i,input[pos+12+8-sa+i]);
    for(const [k,v] of Object.entries({a:0x5040,b:0x5020,x:0x5000,y:0x1111,z:0x2222,s:0xdf00,c:0x3333,p:0x4444}))cpu.reg[k]=v;
    cpu.cc=input[pos+3];[0x46,(sa-1)*16+sb-1,input[pos+2],0x24].forEach((v,i)=>bus.writebyte(0x200+i,v));
    cpu.set_pc(0x200);let ticks=0;cpu.hspre();
    do{cpu.hsstep();ticks++;}while(!(cpu.at_boundry&&cpu.pc===0x204)&&ticks<100000);
    cpu.hsend();assert.ok(ticks<100000,'big divide timeout at '+pos/20);
    for(let i=0;i<8;i++){out[dst+i]=bus.readbyte(0x5000+i);out[dst+8+i]=bus.readbyte(0x5020+i);out[dst+16+i]=bus.readbyte(0x5040+i);}
    for(let i=0;i<8;i++)out.writeUInt16BE(cpu.reg[names[i]],dst+24+2*i);
    out[dst+40]=cpu.cc;out[dst+41]=cpu.level;out.writeUInt16BE(cpu.pc,dst+42);
}
fs.writeFileSync(process.argv[3],out);console.log('PASS execution:',input.length/20,'D/DRM cases');process.exit(0);
