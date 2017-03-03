#ifndef QUATERNION_H_INCLUDED
#define QUATERNION_H_INCLUDED

template <typename F>
class Quaternion
{
private:
    F m_data[4]; //w, x,y,z
public:

    F w() const { return m_data[0]; }
    F x() const { return m_data[1]; }
    F y() const { return m_data[2]; }
    F z() const { return m_data[3]; }
    F &w() { return m_data[0]; }
    F &x() { return m_data[1]; }
    F &y() { return m_data[2]; }
    F &z() { return m_data[3]; }

    Quaternion()
    {
        w() = 1;
        x() = 0;
        y() = 0;
        z() = 0;
    }
    Quaternion(F fw, F fx, F fy, F fz)
    {
        w() = fw;
        x() = fx;
        y() = fy;
        z() = fz;
    }
    //The inverse of a unary quaternion is the conjugate, such as x*Conjugate(x) Equals Identity
    friend Quaternion Conjugate(const Quaternion &a)
    {
        return Quaternion(a.w(), -a.x(), -a.y(), -a.z());
    }

    //The opposed of a unary quaternion is such as x*Opposed(x) equals a full turn
    friend Quaternion Opposed(const Quaternion &a)
    {
        return Quaternion(-a.w(), a.x(), a.y(), a.z());
    }

    friend Quaternion operator*(const Quaternion &p, const Quaternion &q)
    {
        return Quaternion(
            p.w()*q.w() - p.x()*q.x() - p.y()*q.y() - p.z()*q.z(),
            p.x()*q.w() + p.w()*q.x() + p.y()*q.z() - p.z()*q.y(),
            p.y()*q.w() + p.w()*q.y() + p.z()*q.x() - p.x()*q.z(),
            p.z()*q.w() + p.w()*q.z() + p.x()*q.y() - p.y()*q.x()
        );
    }

    void Transform(F &x, F &y, F &z) const
    {
        F aa = this->w()*this->w(), bb = this->x()*this->x(),
          cc = this->y()*this->y(), dd = this->z()*this->z();

        F rx = 2*(this->w()*(this->y()*z - this->z()*y) + this->x()*(this->y()*y + this->z()*z)) +
                   x*(aa + bb - cc - dd);
        F ry = 2*(this->z()*(this->y()*z + this->w()*x) + this->x()*(this->y()*x - this->w()*z)) +
                   y*(aa - bb + cc - dd);
        F rz = 2*(this->x()*(this->w()*y + this->z()*x) + this->y()*(this->z()*y - this->w()*x)) +
                   z*(aa - bb - cc + dd);

        x = rx;
        y = ry;
        z = rz;
    }
    void ToAngles(F &roll, F &pitch, F &yaw) const
    {
        F ysqr = y() * y();

        // roll (x-axis rotation)
        F t0 = +2.0 * (w() * x() + y() * z());
        F t1 = +1.0 - 2.0 * (x() * x() + ysqr);
        roll = atan2(t0, t1);

        // pitch (y-axis rotation)
        F t2 = +2.0 * (w() * y() - z() * x());
        t2 = t2 > 1 ? 1 : t2;
        t2 = t2 < -1 ? -1 : t2;
        pitch = asin(t2);

        // yaw (z-axis rotation)
        F t3 = 2 * (w() * z() + x() * y());
        F t4 = 1 - 2 * (ysqr + z() * z());
        yaw = atan2(t3, t4);
    }
    F AngleHalf() const
    {
        if (w() > F(1.0))
            return 0;
        else if (w() < F(-1.0))
            return F(M_PI);
        else
            return acos(w());
    }
    void ToAxis(F &x, F &y, F &z, F &angle) const
    {
        if (fabs(1-w()) < F(0.0001F) ||
            fabs(1+w()) < F(0.0001F))
        {
            angle = 0;
            x = 1, y = 0, z = 0;
        }
        F angle_2 = AngleHalf();
        angle = 2 * angle_2;

        F si = sin(angle_2); // sqrt(1 - w()*w());
        x = this->x() / si;
        y = this->y() / si;
        z = this->z() / si;
    }

};


#endif /* QUATERNION_H_INCLUDED */
