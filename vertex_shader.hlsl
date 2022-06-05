float4 VS( float4 Pos : POSITION ) : SV_POSITION
{
    // Оставляем координаты точки без изменений
    return Pos;
}