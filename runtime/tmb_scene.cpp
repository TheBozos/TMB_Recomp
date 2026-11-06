// SPDX-License-Identifier: MIT
#include "tmb_port.h"
#include "tmb_scene.h"
#include "bozo/libs/libraries.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace tmb::scene
{
    namespace
    {
        thread_local std::unordered_map<Runtime *, Scene *> scenes;
        using hierarchy::collision::value;
        using hierarchy::collision::bits;
        using hierarchy::collision::sum;
        using hierarchy::collision::difference;
        using hierarchy::collision::negative;
        using Arithmetic = hierarchy::collision::ArithmeticScope;
        float lane(const Vector &v, unsigned i) { return value(v.bits[i]); }
        void lane(Vector &v, unsigned i, float f) { v.bits[i] = bits(f); }
        float moved(float f) { return sum(0.f, f); }
        Basis identity() { return {Vector{{0x3f800000,0,0,0}}, Vector{{0,0x3f800000,0,0}}, Vector{{0,0,0x3f800000,0}}}; }
        Basis transpose(Basis m)
        {
            const auto original = m;
            for (unsigned i = 0; i < 3; ++i) for (unsigned j = 0; j < 3; ++j)
                if (i != j) lane(m[i], j, moved(lane(original[j], i)));
            return m;
        }
        Vector transform(const Basis &m, const Vector &v)
        {
            Vector out;
            for (unsigned i = 0; i < 4; ++i)
                lane(out, i, sum(sum(lane(m[0],i)*lane(v,0), lane(m[1],i)*lane(v,1)), lane(m[2],i)*lane(v,2)));
            return out;
        }
        Basis product(const Basis &m, const Basis &weights)
        { return {transform(m,weights[0]), transform(m,weights[1]), transform(m,weights[2])}; }
        Vector addXYZ(Vector a, const Vector &b)
        { for (unsigned i=0;i<3;++i) lane(a,i,sum(lane(a,i),lane(b,i))); a.bits[3]=0; return a; }
    }

    Binding::Binding(Runtime *rt) : runtime_(rt), scene_(std::make_unique<Scene>())
    {
        if (!scenes.emplace(rt,scene_.get()).second) throw std::runtime_error("duplicate renderer scene binding");
    }
    Binding::~Binding() { scenes.erase(runtime_); }
    Scene &get(Runtime *rt)
    {
        const auto it=scenes.find(rt);
        if(it==scenes.end())throw std::runtime_error("renderer scene has no owner on this game thread");
        return *it->second;
    }
    Frame &Scene::frame(unsigned index)
    {
        if(index>=frames.size() || !frames[index].valid)throw std::runtime_error("uninitialised native hierarchy frame " + std::to_string(index));
        return frames[index];
    }
    void Scene::root()
    {
        const Arithmetic arithmetic;
        frames[0]={identity(),viewOuter,viewInner,viewDirections,true,15};
        position=viewPosition; position.bits[3]=0; selectedFrame=0;
    }
    void Scene::coordinate(const Basis &basis, const Vector &offset)
    {
        const Arithmetic arithmetic;
        frames[0]={basis,product(viewOuter,basis),product(viewInner,basis),product(viewDirections,basis),true,15};
        position=transform(transpose(basis),addXYZ(viewPosition,offset)); position.bits[3]=0; selectedFrame=0;
    }
    void Scene::child(unsigned parent, const Basis &basis, const Vector &offset)
    {
        const Arithmetic arithmetic;
        const Frame old=frame(parent);
        if(parent+1>=frames.size())throw std::runtime_error("native hierarchy depth exceeded");
        frames[parent+1]={product(old.rotation,basis),product(old.outer,basis),product(old.inner,basis),product(old.directions,basis),true,15};
        position=transform(transpose(basis),addXYZ(position,offset));position.bits[3]=0;selectedFrame=parent+1;
    }
    void Scene::fov(unsigned index, const Vector &bounds)
    {
        const Arithmetic arithmetic;
        const auto &f=frame(index);sphere=bounds;selectedFrame=index;
        const auto centre=addXYZ(bounds,position);
        const auto outer=transform(f.outer,centre);nearRange=outer;
        const auto inner=transform(f.inner,centre);
        const float radius=moved(lane(bounds,3));
        const float innerRadius=lane(bounds,3)*1.f;
        auto xy=[](float x,float y){return (negative(x)?0x80u:0u)|(negative(y)?0x40u:0u);};
        outside=xy(sum(lane(inner,0),radius),sum(lane(inner,1),radius));
        if(negative(sum(lane(outer,0),radius))||negative(sum(lane(outer,1),radius))||negative(sum(lane(outer,2),radius)))outside|=2;
        if(negative(sum(value(nearMargin),sum(lane(inner,2),radius))))outside|=0x80;
        inside=xy(difference(lane(inner,0),innerRadius),difference(lane(inner,1),innerRadius));
        if(negative(difference(lane(outer,0),innerRadius))||negative(difference(lane(outer,1),innerRadius))||negative(difference(lane(outer,2),innerRadius)))inside|=2;
    }
    void Scene::project(unsigned index)
    {
        const Arithmetic arithmetic;
        const auto &f=frame(index);selectedFrame=index;
        for(unsigned i=0;i<3;++i)object.column[i]=f.rotation[i];
        object.column[3]=transform(f.rotation,position);object.column[3].bits[3]=0x3f800000;
        objectDirections=f.directions;
    }
    void Scene::translate(const Vector &offset, bool macro)
    {
        if(macro)position=math::combineXYZ(offset,position,false);
        else {const Arithmetic arithmetic;position=addXYZ(position,offset);}
    }
    void Scene::scaledDistance(const Vector &offset,uint32_t scale)
    {
        const Arithmetic arithmetic;
        const auto v=addXYZ(offset,position);
        const float x=lane(v,0),y=lane(v,1),z=lane(v,2);
        distance=bits(sum(sum(x*x,1.f*(y*y)),1.f*(z*z))*value(scale));
    }
    void Scene::selectLights()
    {
        const Arithmetic arithmetic;
        if(pointCount>points.size() || pointWrites!=pointCount)throw std::runtime_error("incomplete native point-light list");
        const Basis inverse=transpose({object.column[0],object.column[1],object.column[2]});
        std::array<Vector,4> directions{},positions{},colours{};
        const auto centre=addXYZ(position,sphere);
        unsigned kept=0;
        for(unsigned i=0;i<pointCount && kept<4;++i)
        {
            const auto &light=points[i];
            const auto lightPosition=transform(inverse,addXYZ(viewPosition,light.sphere));
            const auto direction=transform(inverse,light.direction);
            const float divisor=difference(1.f,lane(light.direction,3)),q=1.f/divisor;
            Vector scaled,fromCentre,fromEye;
            for(unsigned k=0;k<3;++k)
            {
                lane(scaled,k,lane(direction,k)*q);
                lane(fromCentre,k,difference(lane(centre,k),lane(lightPosition,k)));
                lane(fromEye,k,difference(lane(position,k),lane(lightPosition,k)));
            }
            lane(scaled,3,(lane(light.direction,3)*-1.f)*q);
            const float x=lane(fromCentre,0),y=lane(fromCentre,1),z=lane(fromCentre,2);
            const float radius=lane(light.sphere,3),objectRadius=lane(sphere,3);
            // Keep the shipped calculation's scaled-X factor on the Y squared term.
            const float distanceSquared=sum(sum(x*x,lane(scaled,0)*(y*y)),1.f*(z*z));
            if(!negative(difference(distanceSquared,sum(radius*radius,objectRadius*objectRadius))))continue;
            const bool ambient=(ambientMask&(1u<<i))!=0;
            const float facing=sum(sum(sum(y*lane(direction,1),x*lane(direction,0)),1.f*(z*lane(direction,2))),1.f*objectRadius);
            if(!ambient && negative(facing))continue;
            directions[kept]=ambient?Vector{{0,0,0,0x3f800000}}:scaled;
            // The shipped eight-light layout shares its last input colour with the first selected colour.
            // It is cleared before selection, then replaced by the first accepted light; retain that data alias.
            positions[kept]=fromEye;colours[kept]=i==7?colours[0]:light.colour;++kept;
        }
        lighting={};lighting.enabled=kept!=0;lighting.count=kept;
        for(unsigned row=0;row<3;++row)for(unsigned column=0;column<3;++column)
        {
            lane(lighting.data[row],column,row==column?lane(directions[column],row):moved(lane(directions[column],row)));
            lane(lighting.data[4+row],column,row==column?lane(positions[column],row):moved(lane(positions[column],row)));
        }
        for(unsigned i=0;i<3;++i)lane(lighting.data[3],i,moved(lane(directions[i],3)));
        for(unsigned i=0;i<3;++i)
        {
            lighting.data[7+i]=colours[i];lighting.data[7+i].bits[3]=colours[3].bits[i];
            lighting.data[10].bits[i]=colours[i].bits[3];
        }
        lighting.data[10].bits[3]=colours[3].bits[3];
    }
}
