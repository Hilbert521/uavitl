#include <iostream>

#include <math.h>
#include "rigid_control.hh"
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <unsupported/Eigen/KroneckerProduct>

DistanceFormation::DistanceFormation(int m, int l, Eigen::VectorXf d,
        Eigen::VectorXf mu, Eigen::VectorXf tilde_mu,
        Eigen::MatrixXf B,
        float c_shape, float c_vel):
    _m(m),
    _l(l),
    _B(B),
    _d(d),
    _mu(mu),
    _tilde_mu(tilde_mu),
    _c_shape(c_shape),
    _c_vel(c_vel)
{
    _agents = B.rows();
    _edges = B.cols();

    _S1 = Eigen::MatrixXf::Zero(_agents, _edges);
    _S2 = Eigen::MatrixXf::Zero(_agents, _edges);
    _Av = Eigen::MatrixXf::Zero(_agents, _edges);
    _Aa = Eigen::MatrixXf::Zero(_agents, _edges);
    _make_S1_S2();
    _make_Av_Aa();

    Eigen::MatrixXf I2(Eigen::MatrixXf::Identity(_m, _m));
    Eigen::kroneckerProduct(_B, I2, _Bb);
    Eigen::kroneckerProduct(_S1, I2, _S1b);
    Eigen::kroneckerProduct(_S2, I2, _S2b);
    Eigen::kroneckerProduct(_Av, I2, _Avb);
    Eigen::kroneckerProduct(_Aa, I2, _Aab);
}

DistanceFormation::DistanceFormation()
{
}

~DistanceFormation::DistanceFormatio()
{
}

Eigen::VectorXf DistanceFormation::get_u_vel(Eigen::VectorXf X)
{
    Eigen::VectorXf U(Eigen::VectorXf::Zero(X.rows()));
    Eigen::MatrixXf Z = _Bb.transpose()*X;
    if (_l == 1){
        Eigen::MatrixXf Dzh = _make_Dzh(Z);
        Eigen::VectorXf E = _make_E(Z);
        Eigen::VectorXf U = -_c_shape*_Bb*Dzh*E + _Avb*Z;
    }
    else{
        Eigen::MatrixXf Dz = _make_Dz(Z);
        Eigen::MatrixXf Dzt = _make_Dzt(Z);
        Eigen::VectorXf E = _make_E(Z);
        Eigen::VectorXf U = -_c_shape*_Bb*Dz*Dzt*E + _Avb*Z;
    }

    return U;
}

DistanceFormation::get_u_acc(Eigen::VectorXf X, Eigen::VectorXf V)
{
    Eigen::VectorXf U(Eigen::VectorXf::Zero(X.rows()));
    Eigen::MatrixXf Z = _Bb.transpose()*X;
    if (_l == 1){
        Eigen::MatrixXf Dzh = _make_Dzh(Z);
        Eigen::VectorXf E = _make_E(Z);
        Eigen::VectorXf U = -_c_shape*_Bb*Dzh*E + _c_vel*_Avb*Z + _Aab*Z 
            - _c_vel*V;
    }
    else{
        Eigen::MatrixXf Dz = _make_Dz(Z);
        Eigen::MatrixXf Dzt = _make_Dzt(Z);
        Eigen::VectorXf E = _make_E(Z);
        Eigen::VectorXf U = -_c_shape*_Bb*Dz*Dzt*E + _c_vel*_Avb*Z + _Aab*Z
            - _c_vel*V;
    }

    return U;
}

void DistanceFormation::_make_S1_S2()
{
    int i, j;
    for(i = 0; i < _agents; i++)
        for(j = 0; j < _edges; j++){
            if(_B(i, j) == 1)
                _S1(i, j) =  1;
            else if(_B(i, j) == -1)
                _S2(i, j) =  1;
        }
}

void DistanceFormation::_make_Av_Aa()
{
    int i, j;
    for(i = 0; i < _agents; i++)
        for(j = 0; j < _edges; j++){
            if(_B(i, j) == 1)
                _Av(i, j) =  _mu(j);
            else if(_B(i, j) == -1)
                _Av(i, j) =  _tilde_mu(j);
        }

   _Aa = _Av*B.transpose()*_Av;
}

Eigen::MatrixXf DistanceFormation::_make_Dz(Eigen::VectorXf Z)
{
    Eigen::MatrixXf Dz(Eigen::MatrixXf::Zero(z.rows(), _edges));
    int i;

    for(i = 0; i < _edges; i++)
        Dz.block<_m, 1>(i*_m, i) = Z.block<_m, 1>(i*_m, 0);

    return Dz;
}

Eigen::MatrixXf DistanceFormation::_make_Dzt(Eigen::VectorXf Z)
{
    if(_l == 2)
        return Eigen::MatrixXf I2(Eigen::MatrixXf::Identity(_edges, _edges));

    Eigen::VectorXf Zt = Eigen::VectorXf::Zero(_edges);
    int i;

    for(i = 0; i < _edges; i++)
        Zt(i) = pow(Z.block<_m, 1>(i*_m, 0).norm(), _l-2);

    return Zt.asDiagonal();
}

Eigen::MatrixXf DistanceFormation::_make_Dzh(Eigen::VectorXf Z)
{
    Eigen::MatrixXf Dzh(Eigen::MatrixXf::Zero(z.rows(), _edges));
    int i;

    for(i = 0; i < _edges; i++)
        Dz.block<_m, 1>(i*_m, i) = Z.block<_m, 1>(i*_m, 0).normalize();

    return Dzh;
}

Eigen::MatrixXf DistanceFormation::_make_E(Eigen::VectorXf Z)
{
    Eigen::VectorXf E(Eigen::VectorXf::Zero(_edges);
    int i;

    if( _l == 1)
        for(i = 0; i < _edges; i++)
            E(i) = Z.block<_m, 1>(i*_m, 0).norm() - _d(i);
    else
        for(i = 0; i < _edges; i++)
            E(i) = pow(Z.block<_m, 1>(i*_m, 0).norm(), _l) - pow(_d(i), _l);

    return E;
}