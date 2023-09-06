
顶点数据 float 型，xyz 3 个值
    ArrayProperty name=P;interpretation=point;datatype=float32_t[3];arraysize=25921;numsamps=72
面数数据,只存储一份，表示面是几边形，一般是 3/4边形
    ArrayProperty name=.faceCounts;interpretation=;datatype=int32_t;arraysize=25600;numsamps=72
面数据是顶点索引， int 型，4边面 4 个顶点一个面，3角形3个顶点
    ArrayProperty name=.faceIndices;interpretation=;datatype=int32_t;arraysize=102400;numsamps=72
    ArrayProperty name=.indices;interpretation=;datatype=uint32_t;arraysize=102400;numsamps=72
面法线，每个面一个，xyz 
    ArrayProperty name=N;interpretation=normal;datatype=float32_t[3];arraysize=102400;numsamps=45
uv 数据, uv 2 个值，float
    ArrayProperty name=.vals;interpretation=vector;datatype=float32_t[2];arraysize=25921;numsamps=72
曲线 cv 顶点，xyz 
    ArrayProperty name=P;interpretation=point;datatype=float32_t[3];arraysize=100;numsamps=5

