print([3,1,2].sort().join());
print([3,1,2].sort((a,b)=>a-b).join());
print(["b","a","c"].sort().join());
print([3,1,2].toSorted().join());
print([3,1,2].toSorted((a,b)=>a-b).join());
let a=[]; for (let i=0;i<20;i++) a.push((i*7)%20); print(a.sort((x,y)=>x-y).join());
print([1.5,0.5].sort().join());
