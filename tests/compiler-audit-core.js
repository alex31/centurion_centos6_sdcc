'use strict';
const fs=require('node:fs'),path=require('node:path');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');
const {CoreEmulator}=require(root+'/build/out/emulator/server/src/core/emulator');
const machine=new CoreEmulator(),cpu=global.window.cpu,bus=global.window.bpl;
const directory=process.argv[2];
const loadAddress=Number.parseInt(process.env.CPU6_BARE_ORIGIN || '2000',16);
assert.ok(Number.isSafeInteger(loadAddress) && loadAddress >= 0x200 && loadAddress < 0x8000);
const files=JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
assert.ok(files.length > 0);
const results=[];let serial='';
const port=global.window.io_mux.muxports[0],original=port.write_data.bind(port);
port.write_data=v=>{serial+=String.fromCharCode(v&127);return original(v);};
for(const file of files){
 const code=fs.readFileSync(path.join(directory,file));
 assert.equal(code[0x17],0x79); assert.equal(code[0x1a],0);
 assert.ok(loadAddress + code.length <= 0x8000);
 machine.reset();let init=0;
 while(!cpu.at_boundry&&init++<10000)cpu.step(false);
 assert.ok(cpu.at_boundry);
 for(let i=0;i<code.length;i++)bus.writebyte(loadAddress+i,code[i]);
 cpu.set_pc(loadAddress);serial='';
 let ticks=0,reason='timeout',limit=Number(process.env.CPU6_MICROSTEP_LIMIT || 20000000);
 let stackTop=null,minimumStack=null,stackLevel=null;
 assert.ok(Number.isSafeInteger(limit) && limit > 0);
 try{
  for(;ticks<limit;ticks++){
   if(cpu.at_boundry){
    if(stackTop===null&&cpu.pc===loadAddress+0x17){stackTop=minimumStack=cpu.reg.s;stackLevel=cpu.level;}
    if(stackTop!==null&&cpu.level===stackLevel&&cpu.pc>=loadAddress&&cpu.pc<loadAddress+code.length)
     minimumStack=Math.min(minimumStack,cpu.reg.s);
    if(cpu.pc===loadAddress+0x1a){reason='returned';break;}
    if(bus.readbyte(cpu.pc)===0){reason='unexpected halt';break;}
   }
   cpu.step(false);
   if(ticks%50===0){
    for(const d of global.window.__runHW||[])if(d.run)d.run(1);
    for(const d of global.window.__runHSHW||[])if(d.run)d.run(5);
    for(const p of global.window.io_mux.muxports)if(p.run_tx)p.run_tx();
   }
  }
 }catch(e){reason=String(e);}
 const result=reason==='returned'&&cpu.reg.b===0&&serial.includes('PASS')?'PASS':'FAIL';
 results.push({file,result,reason,pc:cpu.pc,b:cpu.reg.b,ticks,serial,
               stackTop,minimumStack,stackBytes:stackTop===null?null:stackTop-minimumStack});
 console.log('AUDIT',file,result,reason,'B='+cpu.reg.b,'PC='+cpu.pc.toString(16),'ticks='+ticks,JSON.stringify(serial));
}
fs.writeFileSync(path.join(directory,'system-results.json'),JSON.stringify(results,null,2));
process.exit(results.every(r=>r.result==='PASS')?0:1);
