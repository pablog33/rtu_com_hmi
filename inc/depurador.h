
/*
@author : SPC SOEO - Proyecto SM13

Nombre función : depurador
Retorna : -

Parámetros : S : Severidad(nivel de test).Variable entera[0, 5] que le indica a
esta función el nivel de penetración en el código.
0 : depurador desactivado
1 : severidad del depurador global
2 : severidad media
3 : severidad particular(ideal para test unitario)
SM : nombre del sub - módulo llamante
Texto : mensaje de información

Descripción : Esta función permite depurar el ME a diferentes
niveles de penetración del código(desde lo global
    hacia lo más particular) mediante la impresión en
    consola de diferentes mensaje de información.

*/


#ifndef depurador_H
#define depurador_H

#include <stdint.h>

#define depurador(eS,sTexto) \
if (eS>= eSeveridad) printf("\n\t ---------------------------------------------- \n At time %lu - %s %d\n\t %s \n",xTaskGetTickCount(),__FILE__, __LINE__,  sTexto)

#define depuradorN(eS, sTexto, iValor) \
if (eS>= eSeveridad) printf("\n\t ---------------------------------------------- \n At time %lu - %s %d\n\t %s %d \n",xTaskGetTickCount(),__FILE__, __LINE__,  sTexto, iValor)

typedef enum {
    eDebug = 0,  
    eInfo, 
    eWarn,
    eError
}severidad_t;

severidad_t eSeveridad;



#pragma once

#endif // !depurador_H




